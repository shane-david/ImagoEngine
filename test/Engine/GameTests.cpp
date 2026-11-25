/**
 * @file GameTests.cpp
 * @brief Smoke tests for Game through a real window: running scenes, applying switch and quit
 * requests at the end of a frame, and shutting down. Every test here is hidden behind [.window].
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "Imago/Engine/EngineControl.hpp"
#include "Imago/Engine/Game.hpp"
#include "Imago/Engine/GameConfig.hpp"
#include "Imago/Engine/Scene.hpp"
#include "Imago/Engine/SceneContext.hpp"

using namespace Imago::Engine;

namespace
{
    using Calls = std::vector<std::string>;

    Calls gCalls;                 ///< scene hooks and system calls, in order
    int gFrames = 0;              ///< frames counted by the update systems
    int gRestartLoads = 0;        ///< how many times RestartScene has loaded
    bool gRestartPersist = false; ///< persist flag RestartScene uses for its self-switch

    void clear_log()
    {
        gCalls.clear();
        gFrames = 0;
        gRestartLoads = 0;
        gRestartPersist = false;
    }

    int count_of(const std::string& entry)
    {
        return static_cast<int>(std::count(gCalls.begin(), gCalls.end(), entry));
    }

    // --- scenes that make their request from on_load ---

    /// @brief Ends the game from on_load.
    class QuitScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            gCalls.push_back("Quit:load");
            _ctx.engine->quit();
        }
        void on_unload() override { gCalls.push_back("Quit:unload"); }
    };

    /// @brief Requests a non-persisting switch to "Quit".
    class SwitchScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            gCalls.push_back("Switch:load");
            _ctx.engine->switch_scene("Quit");
        }
        void on_unload() override { gCalls.push_back("Switch:unload"); }
    };

    /// @brief Requests a switch to "Quit" AND quits in the same frame.
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

    /// @brief Requests a persisting switch to "Quit".
    class PersistSwitchScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            gCalls.push_back("Persist:load");
            _ctx.engine->switch_scene("Quit", true);
        }
        void on_unload() override { gCalls.push_back("Persist:unload"); }
    };

    /// @brief Switches to itself on its first load (a restart), quits on its second.
    class RestartScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            gCalls.push_back("Restart:load");
            ++gRestartLoads;
            if (gRestartLoads == 1) {
                _ctx.engine->switch_scene("Restart", gRestartPersist);
            } else {
                _ctx.engine->quit();
            }
        }
        void on_unload() override { gCalls.push_back("Restart:unload"); }
    };

    // --- scenes that make their request from an update system ---

    /// @brief Counts frames and asks for a switch to "Quit" on the third.
    void switch_on_third_frame(SceneContext& ctx, float)
    {
        gCalls.push_back("SystemSwitch:frame");
        if (++gFrames == 3) {
            ctx.engine->switch_scene("Quit");
        }
    }

    class SystemSwitchScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            gCalls.push_back("SystemSwitch:load");
            _scheduler.register_update(switch_on_third_frame);
        }
        void on_unload() override { gCalls.push_back("SystemSwitch:unload"); }
    };

    /// @brief Asks for a switch and a quit in the same frame from a system.
    void switch_and_quit(SceneContext& ctx, float)
    {
        gCalls.push_back("SystemBoth:frame");
        ctx.engine->switch_scene("Quit");
        ctx.engine->quit();
    }

    class SystemBothScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            gCalls.push_back("SystemBoth:load");
            _scheduler.register_update(switch_and_quit);
        }
        void on_unload() override { gCalls.push_back("SystemBoth:unload"); }
    };

    /// @brief Small window, uncapped so the test never waits on vsync.
    GameConfig make_config(const std::string& firstScene,
                           FramePacing pacing = FramePacing::Uncapped)
    {
        GameConfig config;
        config.title = "ImagoEngine Test";
        config.width = 320;
        config.height = 180;
        config.pacing = pacing;
        config.firstScene = firstScene;
        return config;
    }
}

// Each Game lives in its own braces so it is destroyed (and its scenes unloaded) before the log
// is checked.

TEST_CASE("Game runs the first scene and quits cleanly", "[Game][.window]")
{
    clear_log();
    {
        Game game(make_config("Quit"));
        game.register_scene<QuitScene>("Quit");
        game.run();

        // run() has returned, the scene is still loaded until Game is destroyed
        CHECK(gCalls == Calls{"Quit:load"});
    }

    // destroying Game unloads the active scene exactly once
    CHECK(gCalls == Calls{"Quit:load", "Quit:unload"});
}

TEST_CASE("Game applies a switch_scene request at the end of the frame", "[Game][.window]")
{
    clear_log();
    {
        Game game(make_config("Switch"));
        game.register_scene<SwitchScene>("Switch");
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // Switch is unloaded when it is replaced (persist false), Quit is unloaded when Game is
    // destroyed
    CHECK(gCalls == Calls{"Switch:load", "Switch:unload", "Quit:load", "Quit:unload"});
}

TEST_CASE("Game applies a switch requested by a system after that frame finishes",
          "[Game][.window]")
{
    clear_log();
    {
        Game game(make_config("SystemSwitch"));
        game.register_scene<SystemSwitchScene>("SystemSwitch");
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // exactly three frames ran in SystemSwitch, the switch happened after the third
    CHECK(gCalls == Calls{"SystemSwitch:load", "SystemSwitch:frame", "SystemSwitch:frame",
                          "SystemSwitch:frame", "SystemSwitch:unload", "Quit:load", "Quit:unload"});
}

TEST_CASE("Game drops a pending switch when quit is requested in the same frame", "[Game][.window]")
{
    clear_log();

    SECTION("requested from on_load")
    {
        {
            Game game(make_config("Both"));
            game.register_scene<SwitchAndQuitScene>("Both");
            game.register_scene<QuitScene>("Quit");
            game.run();
        }

        // Quit is never loaded: the quit check comes before apply_pending_switch
        CHECK(gCalls == Calls{"Both:load", "Both:unload"});
    }

    SECTION("requested from a system")
    {
        {
            Game game(make_config("SystemBoth"));
            game.register_scene<SystemBothScene>("SystemBoth");
            game.register_scene<QuitScene>("Quit");
            game.run();
        }

        CHECK(gCalls == Calls{"SystemBoth:load", "SystemBoth:frame", "SystemBoth:unload"});
    }
}

TEST_CASE("Game restarts the active scene when it switches to itself", "[Game][.window]")
{
    clear_log();
    gRestartPersist = GENERATE(false, true);
    {
        Game game(make_config("Restart"));
        game.register_scene<RestartScene>("Restart");
        game.run();
    }

    // the persist flag does not matter when the target is the active scene
    CHECK(gCalls == Calls{"Restart:load", "Restart:unload", "Restart:load", "Restart:unload"});
}

TEST_CASE("Game unloads a persisted inactive scene on shutdown", "[Game][.window]")
{
    clear_log();
    {
        Game game(make_config("Persist"));
        game.register_scene<PersistSwitchScene>("Persist");
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // the persisted scene stays loaded through the switch, so both unloads happen at destruction
    REQUIRE(gCalls.size() == 4);
    CHECK(gCalls[0] == "Persist:load");
    CHECK(gCalls[1] == "Quit:load");
    CHECK(count_of("Persist:unload") == 1);
    CHECK(count_of("Quit:unload") == 1);
}

TEST_CASE("Game constructs and runs under every pacing mode", "[Game][.window]")
{
    const FramePacing mode =
        GENERATE(FramePacing::VSync, FramePacing::Limited, FramePacing::Uncapped);

    clear_log();
    {
        GameConfig config = make_config("Quit", mode);
        config.targetFps = 60; // only read by Limited

        Game game(config);
        game.register_scene<QuitScene>("Quit");
        game.run();
    }

    // pacing cannot be read back from SFML, so this only proves each mode runs without crashing
    CHECK(gCalls == Calls{"Quit:load", "Quit:unload"});
}
