/**
 * @file SceneManagerTests.cpp
 * @brief Tests SceneManager: building scenes on first use, deferred switching, persist (suspend)
 * versus destroy, restarting the active scene, and shutdown.
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

#include "Imago/ECS/Nexus.hpp"
#include "Imago/Engine/InputState.hpp"
#include "Imago/Engine/SceneManager.hpp"

using namespace Imago::Engine;
using Imago::ECS::Entity;

namespace
{
    using Calls = std::vector<std::string>;

    struct Counter {
        int ticks = 0;
    };

    // every test scene and system writes into these, so tests can check what happened and in
    // what order
    Calls gCalls;                     ///< events in the order they happened
    std::vector<float> gDts;          ///< the dt each update system received
    InputState* gSeenInput = nullptr; ///< the input pointer a scene saw in its context
    SceneManager* gManager = nullptr; ///< lets SceneC request a switch from inside on_load
    Imago::ECS::Nexus* gCounterNexus = nullptr; ///< the CounterScene's Nexus
    Entity gCounterEntity = 0;                  ///< the Entity CounterScene ticks

    void clear_log()
    {
        gCalls.clear();
        gDts.clear();
        gSeenInput = nullptr;
        gManager = nullptr;
        gCounterNexus = nullptr;
        gCounterEntity = 0;
    }

    int count_of(const std::string& name)
    {
        return static_cast<int>(std::count(gCalls.begin(), gCalls.end(), name));
    }

    // test systems: free functions, registered by the scenes in on_load
    void update_a(SceneContext&, float dt)
    {
        gCalls.push_back("A:update");
        gDts.push_back(dt);
    }

    void update_b(SceneContext&, float dt)
    {
        gCalls.push_back("B:update");
        gDts.push_back(dt);
    }

    void tick_counter(SceneContext& ctx, float)
    {
        gCalls.push_back("Counter:update");
        ctx.nexus->get<Counter>(gCounterEntity).ticks++;
    }

    // Test scenes. They log their whole lifecycle, including construction and destruction,
    // so tests can tell a suspended scene from a destroyed one and a reactivation from a rebuild.
    class SceneA : public Scene {
    public:
        explicit SceneA(const SceneContext& ctx) : Scene(ctx) { gCalls.push_back("A:created"); }
        ~SceneA() override { gCalls.push_back("A:destroyed"); }

    protected:
        void on_load() override
        {
            gCalls.push_back("A:on_load");
            gSeenInput = _ctx.input;
            _scheduler.register_update(update_a);
        }

        void on_unload() override { gCalls.push_back("A:on_unload"); }
    };

    class SceneB : public Scene {
    public:
        explicit SceneB(const SceneContext& ctx) : Scene(ctx) { gCalls.push_back("B:created"); }
        ~SceneB() override { gCalls.push_back("B:destroyed"); }

    protected:
        void on_load() override
        {
            gCalls.push_back("B:on_load");
            _scheduler.register_update(update_b);
        }

        void on_unload() override { gCalls.push_back("B:on_unload"); }
    };

    /// @brief Inherits its constructor, like editor-generated scenes will, and asks for a switch
    /// to "B" while loading.
    class SceneC : public Scene {
    public:
        using Scene::Scene;
        ~SceneC() override { gCalls.push_back("C:destroyed"); }

    protected:
        void on_load() override
        {
            gCalls.push_back("C:on_load");
            if (gManager != nullptr) {
                gManager->request_switch("B", false);
            }
        }

        void on_unload() override { gCalls.push_back("C:on_unload"); }
    };

    /// @brief Keeps a Counter component in its Nexus that its update system increments.
    class CounterScene : public Scene {
    public:
        explicit CounterScene(const SceneContext& ctx) : Scene(ctx)
        {
            gCalls.push_back("Counter:created");
        }
        ~CounterScene() override { gCalls.push_back("Counter:destroyed"); }

    protected:
        void on_load() override
        {
            gCalls.push_back("Counter:on_load");
            gCounterNexus = &_nexus;
            gCounterEntity = _nexus.create();
            _nexus.bind<Counter>(gCounterEntity, Counter{});
            _scheduler.register_update(tick_counter);
        }

        void on_unload() override { gCalls.push_back("Counter:on_unload"); }
    };

    void register_all(SceneManager& manager)
    {
        manager.register_scene<SceneA>("A");
        manager.register_scene<SceneB>("B");
        manager.register_scene<SceneC>("C");
        manager.register_scene<CounterScene>("Counter");
    }

    /// @brief What the Game does: record a request, then apply it at a safe point.
    void go(SceneManager& manager, const std::string& name, bool persist)
    {
        manager.request_switch(name, persist);
        manager.apply_pending_switch();
    }
}

TEST_CASE("SceneManager cannot be copied", "[SceneManager]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<SceneManager>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<SceneManager>);
}

TEST_CASE("Switching to a registered scene builds and loads it", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);

    CHECK(gCalls == Calls{"A:created", "A:on_load"});
}

TEST_CASE("Nothing happens until apply_pending_switch", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    manager.request_switch("A", false);
    CHECK(gCalls.empty());

    manager.apply_pending_switch();
    CHECK(gCalls == Calls{"A:created", "A:on_load"});
}

TEST_CASE("apply_pending_switch with no request does nothing", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    manager.apply_pending_switch();
    CHECK(gCalls.empty());

    go(manager, "A", false);
    clear_log();

    manager.apply_pending_switch(); // the request was already applied
    CHECK(gCalls.empty());
}

TEST_CASE("apply_pending_switch reports whether a switch was applied", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    // nothing has been requested yet
    CHECK_FALSE(manager.apply_pending_switch());

    manager.request_switch("A", false);
    CHECK(manager.apply_pending_switch());

    // the request was consumed, so the next call has nothing to apply
    CHECK_FALSE(manager.apply_pending_switch());

    SECTION("a switch back to a suspended scene also counts")
    {
        go(manager, "B", true);
        manager.request_switch("A", false);
        CHECK(manager.apply_pending_switch());
        CHECK_FALSE(manager.apply_pending_switch());
    }

    SECTION("a restart of the active scene also counts")
    {
        manager.request_switch("A", false);
        CHECK(manager.apply_pending_switch());
        CHECK_FALSE(manager.apply_pending_switch());
    }
}

TEST_CASE("Scenes are built from the Game's context", "[SceneManager]")
{
    clear_log();
    InputState input;
    SceneContext gameCtx;
    gameCtx.input = &input;

    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);

    CHECK(gSeenInput == &input);
}

TEST_CASE("update forwards to the active scene with the real dt", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    SECTION("with no active scene it does nothing")
    {
        manager.update(0.5f);
        CHECK(gCalls.empty());
    }

    SECTION("with an active scene it updates it")
    {
        go(manager, "A", false);
        clear_log();

        manager.update(0.25f);
        manager.update(0.75f);

        CHECK(gCalls == Calls{"A:update", "A:update"});
        CHECK(gDts == std::vector<float>{0.25f, 0.75f});
    }
}

TEST_CASE("Leaving a scene without persist unloads and destroys it", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);
    clear_log();

    go(manager, "B", false);

    CHECK(gCalls == Calls{"A:on_unload", "A:destroyed", "B:created", "B:on_load"});
}

TEST_CASE("Leaving a scene with persist suspends it", "[SceneManager][Persist]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);
    clear_log();

    go(manager, "B", true); // A is the scene being left, and it persists

    // no unload and no destroy for A
    CHECK(gCalls == Calls{"B:created", "B:on_load"});

    SECTION("only the new scene is updated")
    {
        clear_log();
        manager.update(0.1f);
        CHECK(gCalls == Calls{"B:update"});
    }
}

TEST_CASE("Returning to a suspended scene reactivates it without rebuilding or reloading",
          "[SceneManager][Persist]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);
    go(manager, "B", true);  // A is suspended
    go(manager, "A", false); // B is destroyed, A comes back

    CHECK(gCalls ==
          Calls{"A:created", "A:on_load", "B:created", "B:on_load", "B:on_unload", "B:destroyed"});
    CHECK(count_of("A:created") == 1);
    CHECK(count_of("A:on_load") == 1);

    clear_log();
    manager.update(0.2f);
    CHECK(gCalls == Calls{"A:update"});
}

TEST_CASE("A suspended scene keeps its Nexus state and resumes where it left off",
          "[SceneManager][Persist]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "Counter", false);
    manager.update(0.1f);
    manager.update(0.1f);
    REQUIRE(gCounterNexus != nullptr);
    REQUIRE(gCounterNexus->get<Counter>(gCounterEntity).ticks == 2);

    go(manager, "B", true); // Counter is suspended
    manager.update(0.1f);   // only B runs
    CHECK(gCounterNexus->get<Counter>(gCounterEntity).ticks == 2);

    go(manager, "Counter", false); // B is destroyed, Counter resumes
    manager.update(0.1f);

    CHECK(gCounterNexus->get<Counter>(gCounterEntity).ticks == 3);
    CHECK(count_of("Counter:created") == 1);
    CHECK(count_of("Counter:on_load") == 1);
    CHECK(count_of("Counter:on_unload") == 0);
}

TEST_CASE("Switching back and forth with persist builds each scene once", "[SceneManager][Persist]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);
    go(manager, "B", true); // A suspended
    go(manager, "A", true); // B suspended, A reactivated
    go(manager, "B", true); // A suspended, B reactivated
    go(manager, "A", true); // B suspended, A reactivated

    CHECK(gCalls == Calls{"A:created", "A:on_load", "B:created", "B:on_load"});

    clear_log();
    manager.update(0.1f);
    CHECK(gCalls == Calls{"A:update"});
}

TEST_CASE("Switching to the active scene restarts it, whatever the persist flag", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);
    clear_log();

    SECTION("persist false")
    {
        go(manager, "A", false);
    }

    SECTION("persist true")
    {
        go(manager, "A", true);
    }

    CHECK(gCalls == Calls{"A:on_unload", "A:destroyed", "A:created", "A:on_load"});
}

TEST_CASE("Restarting the active scene leaves a suspended scene alone", "[SceneManager][Persist]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "Counter", false);
    manager.update(0.1f);
    Imago::ECS::Nexus* counterNexus = gCounterNexus;
    Entity counterEntity = gCounterEntity;

    go(manager, "B", true); // Counter suspended
    clear_log();

    go(manager, "B", false); // restart B

    CHECK(gCalls == Calls{"B:on_unload", "B:destroyed", "B:created", "B:on_load"});

    go(manager, "Counter", false); // B destroyed, Counter resumes without a rebuild
    CHECK(count_of("Counter:created") == 0);
    CHECK(count_of("Counter:on_load") == 0);
    CHECK(counterNexus->get<Counter>(counterEntity).ticks == 1);
}

TEST_CASE("When several requests arrive in one frame, the last one wins", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    SECTION("different targets")
    {
        manager.request_switch("A", false);
        manager.request_switch("B", false);
        manager.apply_pending_switch();

        CHECK(gCalls == Calls{"B:created", "B:on_load"});
    }

    SECTION("the last persist flag is the one used")
    {
        go(manager, "A", false);
        clear_log();

        manager.request_switch("B", false);
        manager.request_switch("B", true); // A persists
        manager.apply_pending_switch();

        CHECK(gCalls == Calls{"B:created", "B:on_load"});
    }
}

TEST_CASE("A request made during a switch is kept for the next apply", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);
    gManager = &manager;

    go(manager, "C", false); // C asks for a switch to B while loading

    // the switch to C finished untouched, and B has not been built yet
    REQUIRE(gCalls == Calls{"C:on_load"});

    CHECK(manager.apply_pending_switch());

    CHECK(gCalls == Calls{"C:on_load", "C:on_unload", "C:destroyed", "B:created", "B:on_load"});
}

TEST_CASE("The destructor unloads and destroys every live scene", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;

    SECTION("with only an active scene")
    {
        {
            SceneManager manager(gameCtx);
            register_all(manager);

            go(manager, "A", false);
            clear_log();
        }

        CHECK(gCalls == Calls{"A:on_unload", "A:destroyed"});
    }

    SECTION("with an active scene and a suspended one")
    {
        {
            SceneManager manager(gameCtx);
            register_all(manager);

            go(manager, "A", false);
            go(manager, "B", true); // A is suspended, B is active
            clear_log();
        }

        // every live scene is unloaded exactly once, in no particular order, before any is
        // destroyed
        REQUIRE(gCalls.size() == 4);
        CHECK(count_of("A:on_unload") == 1);
        CHECK(count_of("B:on_unload") == 1);
        CHECK(count_of("A:destroyed") == 1);
        CHECK(count_of("B:destroyed") == 1);
        CHECK(gCalls[0].find(":on_unload") != std::string::npos);
        CHECK(gCalls[1].find(":on_unload") != std::string::npos);
    }

    SECTION("a manager that never switched destroys cleanly")
    {
        {
            SceneManager manager(gameCtx);
            register_all(manager);
        }

        CHECK(gCalls.empty());
    }
}
