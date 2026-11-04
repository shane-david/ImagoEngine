/**
 * @file GameTests.cpp
 * @author Shane David
 * @brief Tests EngineControl directly and smoke tests Game through a real window (hidden behind the [window] tag)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "Imago/Engine/EngineControl.hpp"
#include "Imago/Engine/Game.hpp"
#include "Imago/Engine/GameConfig.hpp"
#include "Imago/Engine/Scene.hpp"

using namespace Imago::Engine;

// ---------------------------------------------------------------------------
// test access: the friend struct declared in EngineControl.hpp
// NOTE: define this in ONE test file only, a second definition is an ODR problem
// ---------------------------------------------------------------------------

namespace Imago::Engine
{
    struct EngineControlTestAccess {
        static bool has_pending(const EngineControl& e) { return e._pendingSwitch.has_value(); }
        static auto take(EngineControl& e) { return e.take_switch_request(); }
        static bool quit_requested(const EngineControl& e) { return e.quit_requested(); }
    };
}

namespace
{
    // file-scope call log, in an anonymous namespace so it cannot collide with other test files
    std::vector<std::string> gCalls;

    // --- small test scenes, each logs its hooks and makes one request from on_load ---

    /// ends the game from on_load
    class QuitScene : public Scene {
    public:
        using Scene::Scene;
    protected:
        void on_load() override { gCalls.push_back("Quit:load"); _ctx.engine->quit(); }
        void on_unload() override { gCalls.push_back("Quit:unload"); }
    };

    /// requests a non-persisting switch to "Quit"
    class SwitchScene : public Scene {
    public:
        using Scene::Scene;
    protected:
        void on_load() override { gCalls.push_back("Switch:load"); _ctx.engine->switch_scene("Quit"); }
        void on_unload() override { gCalls.push_back("Switch:unload"); }
    };

    /// requests a switch to "Quit" AND quits in the same frame
    class SwitchAndQuitScene : public Scene {
    public:
        using Scene::Scene;
    protected:
        void on_load() override
        {
            gCalls.push_back("Both:load");
            _ctx.engine->switch_scene("Quit");
            _ctx.engine->quit();
        }
        void on_unload() override { gCalls.push_back("Both:unload"); }
    };

    /// requests a persisting switch to "Quit"
    class PersistSwitchScene : public Scene {
    public:
        using Scene::Scene;
    protected:
        void on_load() override { gCalls.push_back("Persist:load"); _ctx.engine->switch_scene("Quit", true); }
        void on_unload() override { gCalls.push_back("Persist:unload"); }
    };

    /// small window, uncapped so the test never waits on vsync
    GameConfig make_config(const std::string& firstScene, FramePacing pacing = FramePacing::Uncapped)
    {
        GameConfig config;
        config.title = "ImagoEngine Test";
        config.width = 320;
        config.height = 180;
        config.pacing = pacing;
        config.firstScene = firstScene;
        return config;
    }

    int count_of(const std::string& entry)
    {
        return static_cast<int>(std::count(gCalls.begin(), gCalls.end(), entry));
    }
}

// ---------------------------------------------------------------------------
// EngineControl (headless, runs every time)
// ---------------------------------------------------------------------------

TEST_CASE("EngineControl starts with nothing requested", "[EngineControl]")
{
    EngineControl engine;

    REQUIRE_FALSE(EngineControlTestAccess::has_pending(engine));
    REQUIRE_FALSE(EngineControlTestAccess::quit_requested(engine));
    REQUIRE_FALSE(EngineControlTestAccess::take(engine).has_value());
}

TEST_CASE("EngineControl switch_scene records the name and defaults persist to false", "[EngineControl]")
{
    EngineControl engine;

    engine.switch_scene("Level1");

    const auto request = EngineControlTestAccess::take(engine);
    REQUIRE(request.has_value());
    REQUIRE(request->name == "Level1");
    REQUIRE(request->persist == false);
}

TEST_CASE("EngineControl switch_scene records persist when asked", "[EngineControl]")
{
    EngineControl engine;

    engine.switch_scene("Pause", true);

    const auto request = EngineControlTestAccess::take(engine);
    REQUIRE(request.has_value());
    REQUIRE(request->name == "Pause");
    REQUIRE(request->persist == true);
}

TEST_CASE("EngineControl last switch_scene call in a frame wins", "[EngineControl]")
{
    EngineControl engine;

    engine.switch_scene("First", true);
    engine.switch_scene("Second", false);

    const auto request = EngineControlTestAccess::take(engine);
    REQUIRE(request.has_value());
    REQUIRE(request->name == "Second");
    REQUIRE(request->persist == false);
}

TEST_CASE("EngineControl take_switch_request consumes the request once", "[EngineControl]")
{
    EngineControl engine;
    engine.switch_scene("Level1");

    REQUIRE(EngineControlTestAccess::take(engine).has_value());
    REQUIRE_FALSE(EngineControlTestAccess::has_pending(engine));
    REQUIRE_FALSE(EngineControlTestAccess::take(engine).has_value());
}

TEST_CASE("EngineControl quit sets the flag and is idempotent", "[EngineControl]")
{
    EngineControl engine;

    engine.quit();
    REQUIRE(EngineControlTestAccess::quit_requested(engine));

    engine.quit();
    REQUIRE(EngineControlTestAccess::quit_requested(engine));
}

TEST_CASE("EngineControl quit and take do not affect each other", "[EngineControl]")
{
    EngineControl engine;

    SECTION("quit leaves a pending switch alone")
    {
        engine.switch_scene("Level1");
        engine.quit();

        REQUIRE(EngineControlTestAccess::has_pending(engine));
    }

    SECTION("taking a switch does not clear quit")
    {
        engine.quit();
        engine.switch_scene("Level1");
        EngineControlTestAccess::take(engine);

        REQUIRE(EngineControlTestAccess::quit_requested(engine));
    }
}

// the empty-name check is an assert in Debug, which would abort the test run, so it only runs in Release
#ifdef NDEBUG
TEST_CASE("EngineControl ignores an empty scene name in Release", "[EngineControl]")
{
    EngineControl engine;

    engine.switch_scene("");

    REQUIRE_FALSE(EngineControlTestAccess::has_pending(engine));
}
#endif

// ---------------------------------------------------------------------------
// Game smoke tests (need a real window, hidden: run with the [window] tag)
// Each scene's on_load makes its request, so run() ends without any scheduled systems.
// Each Game lives in its own braces so it is destroyed (and its scenes unloaded) before we check the log.
// ---------------------------------------------------------------------------

TEST_CASE("Game runs the first scene and quits cleanly", "[Game][.window]")
{
    gCalls.clear();
    {
        Game game(make_config("Quit"));
        game.register_scene<QuitScene>("Quit");
        game.run();

        // run() has returned, the scene is still loaded until Game is destroyed
        REQUIRE(gCalls == std::vector<std::string>{"Quit:load"});
    }

    // destroying Game unloads the active scene exactly once
    REQUIRE(gCalls == std::vector<std::string>{"Quit:load", "Quit:unload"});
}

TEST_CASE("Game applies a switch_scene request at the end of the frame", "[Game][.window]")
{
    gCalls.clear();
    {
        Game game(make_config("Switch"));
        game.register_scene<SwitchScene>("Switch");
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // Switch is unloaded when it is replaced (persist false), Quit is unloaded when Game is destroyed
    REQUIRE(gCalls == std::vector<std::string>{"Switch:load", "Switch:unload", "Quit:load", "Quit:unload"});
}

TEST_CASE("Game drops a pending switch when quit is requested in the same frame", "[Game][.window]")
{
    gCalls.clear();
    {
        Game game(make_config("Both"));
        game.register_scene<SwitchAndQuitScene>("Both");
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // Quit is never loaded: the quit check comes before apply_pending_switch
    REQUIRE(gCalls == std::vector<std::string>{"Both:load", "Both:unload"});
}

TEST_CASE("Game unloads a persisted inactive scene on shutdown", "[Game][.window]")
{
    gCalls.clear();
    {
        Game game(make_config("Persist"));
        game.register_scene<PersistSwitchScene>("Persist");
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // the persisted scene stays loaded through the switch, so both unloads happen at destruction
    REQUIRE(gCalls.size() == 4);
    REQUIRE(gCalls[0] == "Persist:load");
    REQUIRE(gCalls[1] == "Quit:load");
    REQUIRE(count_of("Persist:unload") == 1);
    REQUIRE(count_of("Quit:unload") == 1);
}

TEST_CASE("Game constructs and runs under every pacing mode", "[Game][.window]")
{
    const FramePacing mode = GENERATE(FramePacing::VSync, FramePacing::Limited, FramePacing::Uncapped);

    gCalls.clear();
    {
        GameConfig config = make_config("Quit", mode);
        config.targetFps = 60; // only read by Limited

        Game game(config);
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // pacing cannot be read back from SFML, so this only proves each mode runs without crashing
    REQUIRE(gCalls == std::vector<std::string>{"Quit:load", "Quit:unload"});
}