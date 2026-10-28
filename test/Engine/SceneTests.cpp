#include <catch2/catch_test_macros.hpp>

#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Imago/Engine/Scene.hpp"

using namespace Imago::Engine;
using namespace Imago::ECS;

namespace
{
    using Calls = std::vector<std::string>;

    struct Marker { int value; };
    struct Ping   { int value; };

    // every test system and hook writes into these, so tests can check what ran and with what
    Calls gCalls;                          // names of what ran, in order
    std::vector<float> gDts;               // the dt each count_system call received
    SceneContext* gSeenCtx = nullptr;      // the SceneContext the last remember_ctx call received
    Entity gTarget = 0;                    // an Entity the bind_during_tick system works on
    bool gHadMarkerDuringTick = true;      // whether the Entity had a Marker while the tick was running
    Entity gCreated = 0;                   // the Entity create_entity_system made

    void clear_log()
    {
        gCalls.clear();
        gDts.clear();
        gSeenCtx = nullptr;
        gTarget = 0;
        gHadMarkerDuringTick = true;
        gCreated = 0;
    }

    // test systems: free functions, since system types are function pointers
    void count_system(SceneContext&, float dt)
    {
        gCalls.push_back("count");
        gDts.push_back(dt);
    }

    void remember_ctx(SceneContext& ctx, float)
    {
        gSeenCtx = &ctx;
    }

    // binds a Marker during the tick and records whether it was visible right away
    void bind_during_tick(SceneContext& ctx, float)
    {
        ctx.nexus->bind<Marker>(gTarget, Marker{ 3 });
        gHadMarkerDuringTick = ctx.nexus->has<Marker>(gTarget);
    }

    // creates an Entity through the context's nexus pointer
    void create_entity_system(SceneContext& ctx, float)
    {
        gCreated = ctx.nexus->create();
        ctx.nexus->bind<Marker>(gCreated, Marker{ 1 });
    }

    // emits through the context's broadcaster pointer
    void emit_ping_system(SceneContext& ctx, float)
    {
        ctx.broadcaster->emit(Ping{ 5 });
    }

    void on_ping(SceneContext&, const Ping& e)
    {
        gCalls.push_back("ping:" + std::to_string(e.value));
    }

    // A Scene is abstract, so tests derive one. Its hooks do whatever the test switches on,
    // and its accessors expose the protected members.
    class TestScene : public Scene {
    public:
        using Scene::Scene;

        // test access to the protected members
        Nexus& nexus() { return _nexus; }
        SceneContext& context() { return _ctx; }
        Broadcaster& broadcaster() { return _broadcaster; }
        SystemScheduler& scheduler() { return _scheduler; }

        // switches the tests set to control the hooks
        bool bindOnLoad = false;
        bool registerSystemOnLoad = false;
        bool bindOnUnload = false;

        Entity loadedEntity = 0;
        Entity unloadedEntity = 0;

    protected:
        void on_load() override
        {
            gCalls.push_back("on_load");

            if (bindOnLoad) {
                loadedEntity = _nexus.create();
                _nexus.bind<Marker>(loadedEntity, Marker{ 7 });
            }
            if (registerSystemOnLoad) {
                _scheduler.register_update(count_system);
            }
        }

        void on_unload() override
        {
            gCalls.push_back("on_unload");

            if (bindOnUnload) {
                unloadedEntity = _nexus.create();
                _nexus.bind<Marker>(unloadedEntity, Marker{ 9 });
            }
        }
    };

    // only overrides on_load, to show that on_unload is optional
    class MinimalScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override {}
    };
}

TEST_CASE("Scene has the shape the design relies on", "[Scene]")
{
    STATIC_REQUIRE(std::is_abstract_v<Scene>);
    STATIC_REQUIRE(std::has_virtual_destructor_v<Scene>);

    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<TestScene>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<TestScene>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<TestScene>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<TestScene>);
}

TEST_CASE("A Scene wires its own nexus and broadcaster into its context", "[Scene]")
{
    clear_log();
    std::unordered_map<std::string, bool> input;
    SceneContext gameCtx;
    gameCtx.input = &input;

    TestScene scene(gameCtx);

    REQUIRE(scene.context().nexus == &scene.nexus());
    REQUIRE(scene.context().broadcaster == &scene.broadcaster());

    // the Game's persistent pointers are copied over
    REQUIRE(scene.context().input == &input);

    // the Game's own context is not modified
    REQUIRE(gameCtx.nexus == nullptr);
    REQUIRE(gameCtx.broadcaster == nullptr);
}

TEST_CASE("Two Scenes built from the same Game context stay independent", "[Scene]")
{
    clear_log();
    SceneContext gameCtx;

    TestScene a(gameCtx);
    TestScene b(gameCtx);

    REQUIRE(a.context().nexus != b.context().nexus);
    REQUIRE(a.context().broadcaster != b.context().broadcaster);

    SECTION("systems receive their own scene's context")
    {
        a.scheduler().register_update(remember_ctx);
        b.scheduler().register_update(remember_ctx);

        a.update(0.1f);
        REQUIRE(gSeenCtx == &a.context());

        b.update(0.1f);
        REQUIRE(gSeenCtx == &b.context());
    }

    SECTION("a system's changes go into its own scene's nexus")
    {
        a.scheduler().register_update(create_entity_system);

        a.update(0.1f);

        REQUIRE(a.nexus().has<Marker>(gCreated));
        REQUIRE_FALSE(b.nexus().has<Marker>(gCreated));
    }
}

TEST_CASE("A system's emit reaches its own scene's Broadcaster", "[Scene]")
{
    clear_log();
    SceneContext gameCtx;

    TestScene a(gameCtx);
    TestScene b(gameCtx);

    a.broadcaster().subscribe<Ping>(on_ping);
    a.scheduler().register_update(emit_ping_system);
    b.scheduler().register_update(emit_ping_system);

    SECTION("scene a has the subscriber")
    {
        a.update(0.1f);
        REQUIRE(gCalls == Calls{ "ping:5" });
    }

    SECTION("scene b has no subscriber, so nothing runs")
    {
        b.update(0.1f);
        REQUIRE(gCalls.empty());
    }
}

TEST_CASE("load runs on_load and then applies what it deferred", "[Scene]")
{
    clear_log();
    SceneContext gameCtx;
    TestScene scene(gameCtx);

    SECTION("components bound in on_load exist once load returns")
    {
        scene.bindOnLoad = true;

        scene.load();

        REQUIRE(gCalls == Calls{ "on_load" });
        REQUIRE(scene.nexus().has<Marker>(scene.loadedEntity));
        REQUIRE(scene.nexus().get<Marker>(scene.loadedEntity).value == 7);
    }

    SECTION("systems registered in on_load run on the next update")
    {
        scene.registerSystemOnLoad = true;

        scene.load();
        scene.update(0.5f);

        REQUIRE(gCalls == Calls{ "on_load", "count" });
        REQUIRE(gDts == std::vector<float>{ 0.5f });
    }
}

TEST_CASE("unload runs on_unload and then applies what it deferred", "[Scene]")
{
    clear_log();
    SceneContext gameCtx;

    SECTION("an overriding scene")
    {
        TestScene scene(gameCtx);
        scene.bindOnUnload = true;

        scene.unload();

        REQUIRE(gCalls == Calls{ "on_unload" });
        REQUIRE(scene.nexus().has<Marker>(scene.unloadedEntity));
    }

    SECTION("a scene that does not override on_unload is harmless")
    {
        MinimalScene scene(gameCtx);

        scene.load();
        scene.unload();

        SUCCEED();
    }
}

TEST_CASE("update ticks the scheduler with the real dt", "[Scene]")
{
    clear_log();
    SceneContext gameCtx;
    TestScene scene(gameCtx);

    scene.scheduler().register_update(count_system);

    scene.update(0.25f);
    scene.update(0.75f);

    REQUIRE(gCalls == Calls{ "count", "count" });
    REQUIRE(gDts == std::vector<float>{ 0.25f, 0.75f });
}

TEST_CASE("Structural changes made during a tick only take effect when update ends", "[Scene]")
{
    clear_log();
    SceneContext gameCtx;
    TestScene scene(gameCtx);

    gTarget = scene.nexus().create();
    scene.scheduler().register_update(bind_during_tick);

    scene.update(0.1f);

    // the system did not see its own bind while the tick was running
    REQUIRE_FALSE(gHadMarkerDuringTick);

    // but update flushed at the end, so it exists now
    REQUIRE(scene.nexus().has<Marker>(gTarget));
    REQUIRE(scene.nexus().get<Marker>(gTarget).value == 3);
}