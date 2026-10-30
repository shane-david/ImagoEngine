#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Imago/Engine/SceneManager.hpp"

using namespace Imago::Engine;

namespace
{
    using Calls = std::vector<std::string>;

    // every test scene and system writes into these, so tests can check what happened and in what order
    Calls gCalls;                                          // events in the order they happened
    std::vector<float> gDts;                               // the dt each update system received
    std::unordered_map<std::string, bool>* gSeenInput = nullptr; // the input pointer a scene saw in its context
    SceneManager* gManager = nullptr;                      // lets SceneC request a switch from inside on_load

    void clear_log()
    {
        gCalls.clear();
        gDts.clear();
        gSeenInput = nullptr;
        gManager = nullptr;
    }

    int count_of(const std::string& name)
    {
        return static_cast<int>(std::count(gCalls.begin(), gCalls.end(), name));
    }

    // test systems: free functions, registered by the scenes in on_load
    void update_a(SceneContext&, float dt) { gCalls.push_back("A:update"); gDts.push_back(dt); }
    void update_b(SceneContext&, float dt) { gCalls.push_back("B:update"); gDts.push_back(dt); }

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

    // inherits its constructor, like editor-generated scenes will, and asks for a switch to "B" while loading
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

    void register_all(SceneManager& manager)
    {
        manager.register_scene<SceneA>("A");
        manager.register_scene<SceneB>("B");
        manager.register_scene<SceneC>("C");
    }

    // what the Game will do: record a request, then apply it at a safe point
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

    REQUIRE(gCalls == Calls{ "A:created", "A:on_load" });
}

TEST_CASE("Nothing happens until apply_pending_switch", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    manager.request_switch("A", false);
    REQUIRE(gCalls.empty());

    manager.apply_pending_switch();
    REQUIRE(gCalls == Calls{ "A:created", "A:on_load" });
}

TEST_CASE("apply_pending_switch with no request does nothing", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    manager.apply_pending_switch();
    REQUIRE(gCalls.empty());

    go(manager, "A", false);
    clear_log();

    manager.apply_pending_switch(); // the request was already applied
    REQUIRE(gCalls.empty());
}

TEST_CASE("Scenes are built from the Game's context", "[SceneManager]")
{
    clear_log();
    std::unordered_map<std::string, bool> input;
    SceneContext gameCtx;
    gameCtx.input = &input;

    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);

    REQUIRE(gSeenInput == &input);
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
        REQUIRE(gCalls.empty());
    }

    SECTION("with an active scene it updates it")
    {
        go(manager, "A", false);
        clear_log();

        manager.update(0.25f);
        manager.update(0.75f);

        REQUIRE(gCalls == Calls{ "A:update", "A:update" });
        REQUIRE(gDts == std::vector<float>{ 0.25f, 0.75f });
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

    REQUIRE(gCalls == Calls{ "A:on_unload", "A:destroyed", "B:created", "B:on_load" });
}

TEST_CASE("Leaving a scene with persist suspends it", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);
    clear_log();

    go(manager, "B", true); // A is the scene being left, and it persists

    // no unload and no destroy for A
    REQUIRE(gCalls == Calls{ "B:created", "B:on_load" });

    SECTION("only the new scene is updated")
    {
        clear_log();
        manager.update(0.1f);
        REQUIRE(gCalls == Calls{ "B:update" });
    }
}

TEST_CASE("Returning to a suspended scene reactivates it without rebuilding or reloading", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    go(manager, "A", false);
    go(manager, "B", true);   // A is suspended
    go(manager, "A", false);  // B is destroyed, A comes back

    REQUIRE(gCalls == Calls{
        "A:created", "A:on_load",
        "B:created", "B:on_load",
        "B:on_unload", "B:destroyed"
    });
    REQUIRE(count_of("A:created") == 1);
    REQUIRE(count_of("A:on_load") == 1);

    clear_log();
    manager.update(0.2f);
    REQUIRE(gCalls == Calls{ "A:update" });
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

    REQUIRE(gCalls == Calls{ "A:on_unload", "A:destroyed", "A:created", "A:on_load" });
}

TEST_CASE("When several requests arrive in one frame, the last one wins", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;
    SceneManager manager(gameCtx);
    register_all(manager);

    manager.request_switch("A", false);
    manager.request_switch("B", false);
    manager.apply_pending_switch();

    REQUIRE(gCalls == Calls{ "B:created", "B:on_load" });
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
    REQUIRE(gCalls == Calls{ "C:on_load" });

    manager.apply_pending_switch();

    REQUIRE(gCalls == Calls{ "C:on_load", "C:on_unload", "C:destroyed", "B:created", "B:on_load" });
}

TEST_CASE("The destructor unloads the active scene and destroys every scene", "[SceneManager]")
{
    clear_log();
    SceneContext gameCtx;

    SECTION("with an active scene and a suspended one")
    {
        {
            SceneManager manager(gameCtx);
            register_all(manager);

            go(manager, "A", false);
            go(manager, "B", true); // B active... A was left with persist false and destroyed
            clear_log();
        }

        // only the active scene gets the unload hook
        REQUIRE(count_of("B:on_unload") == 1);
        REQUIRE(count_of("B:destroyed") == 1);
        REQUIRE(gCalls.front() == "B:on_unload");
    }

    SECTION("a suspended scene is destroyed without any hooks")
    {
        {
            SceneManager manager(gameCtx);
            register_all(manager);

            go(manager, "A", false);
            go(manager, "B", true);  // A is left with persist false here and is destroyed
            go(manager, "A", true);  // B is suspended, A is built again and active
            clear_log();
        }

        REQUIRE(count_of("A:on_unload") == 1);
        REQUIRE(count_of("B:on_unload") == 0);
        REQUIRE(count_of("A:destroyed") == 1);
        REQUIRE(count_of("B:destroyed") == 1);
        REQUIRE(gCalls.front() == "A:on_unload");
    }

    SECTION("a manager that never switched destroys cleanly")
    {
        {
            SceneManager manager(gameCtx);
            register_all(manager);
        }

        REQUIRE(gCalls.empty());
    }
}