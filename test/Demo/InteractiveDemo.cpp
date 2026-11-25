/**
 * @file InteractiveDemo.cpp
 * @brief A small interactive tour of the user-facing ImagoEngine API, built as its own executable
 * (imago_demo).
 *
 * It shows a GameConfig, three registered scenes, entities and components, update and fixed
 * systems, a survey and a bond, an event with a subscriber, input actions bound to keyboard and
 * gamepad (held / began / ended / strength / axis / direction), switch_scene with and without
 * persist, restarting a scene, and quit.
 *
 * NOTE: there is no renderer yet and Game clears the window after systems run, so nothing is
 * drawn. State is shown in the window title (ctx.window->setTitle) and on the console (spdlog).
 */

#include <cmath>
#include <iterator> // for std::size
#include <string>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "Imago/ECS/Nexus.hpp"
#include "Imago/Engine/Broadcaster.hpp"
#include "Imago/Engine/EngineControl.hpp"
#include "Imago/Engine/Game.hpp"
#include "Imago/Engine/GameConfig.hpp"
#include "Imago/Engine/InputSource.hpp"
#include "Imago/Engine/InputState.hpp"
#include "Imago/Engine/Scene.hpp"
#include "Imago/Engine/SceneContext.hpp"

using namespace Imago::Engine;
using Imago::ECS::Entity;
using Scan = sf::Keyboard::Scancode;

namespace
{
    // ------------------------------------------------------------------
    // Components (plain data) and an event
    // ------------------------------------------------------------------

    struct Position {
        glm::vec2 value{0.0f};
    };

    struct Velocity {
        glm::vec2 value{0.0f};
    };

    struct Player {};

    struct Coin {
        int value = 1;
    };

    struct Stats {
        int score = 0;
        int jumps = 0;
        int coinsLeft = 0;
    };

    /// @brief Emitted by coin_system when the player touches a coin.
    struct CoinCollected {
        Entity coin;
        int value;
    };

    // ------------------------------------------------------------------
    // Tuning
    // ------------------------------------------------------------------

    constexpr float WORLD_WIDTH = 960.0f;
    constexpr float WORLD_HEIGHT = 540.0f;
    constexpr float PLAYER_SPEED = 200.0f; // units per second at full stick, doubled by sprint
    constexpr float PICKUP_RADIUS = 30.0f;
    constexpr float TITLE_INTERVAL = 0.1f; // seconds between title refreshes

    // how many times each scene has been loaded, so restarts and resumes are visible
    int gMenuLoads = 0;
    int gPlayLoads = 0;
    int gPauseLoads = 0;

    void print_controls()
    {
        spdlog::info("===================== ImagoEngine demo controls =====================");
        spdlog::info("  Move        WASD / arrow keys / left stick / D-pad   (direction, axis)");
        spdlog::info("  Sprint      Left Shift / right trigger               (strength 0-1)");
        spdlog::info("  Jump        Space / A (bottom face)                  (began/ended/held)");
        spdlog::info("  Confirm     Enter / Start                            (Menu -> Play)");
        spdlog::info("  Pause       P / Back        switch_scene(\"Pause\", persist = true)");
        spdlog::info("  Restart     R / Y (top face)  switch_scene to the active scene");
        spdlog::info("  Menu        M / X (left face) switch_scene(\"Menu\") without persist");
        spdlog::info("  Quit        Esc / B (right face)                     quit()");
        spdlog::info("Nothing is drawn: watch the window title and this console.");
        spdlog::info("=====================================================================");
    }

    /// @brief Returns true once every TITLE_INTERVAL seconds of accumulated dt.
    bool title_due(float& timer, float dt)
    {
        timer += dt;
        if (timer < TITLE_INTERVAL) {
            return false;
        }
        timer = 0.0f;
        return true;
    }

    // ------------------------------------------------------------------
    // Menu scene
    // ------------------------------------------------------------------

    void menu_system(SceneContext& ctx, float dt)
    {
        static float timer = TITLE_INTERVAL;
        if (title_due(timer, dt)) {
            ctx.window->setTitle(
                fmt::format("ImagoEngine demo | MENU (load #{}) | Enter/Start: play   Esc/B: quit",
                            gMenuLoads));
        }

        if (ctx.input->began("confirm")) {
            spdlog::info("[Menu] confirm began -> switch_scene(\"Play\")");
            ctx.engine->switch_scene("Play");
        }
        if (ctx.input->began("quit")) {
            spdlog::info("[Menu] quit began -> quit()");
            ctx.engine->quit();
        }
    }

    class MenuScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            ++gMenuLoads;
            spdlog::info("[Menu] on_load (load #{})", gMenuLoads);
            spdlog::info("[Menu] press Enter / Start to play, Esc / B to quit");
            _scheduler.register_update(menu_system);
        }

        void on_unload() override { spdlog::info("[Menu] on_unload"); }
    };

    // ------------------------------------------------------------------
    // Play scene
    // ------------------------------------------------------------------

    /// @brief Finds the single Player entity with a survey. Returns NULL_ENTITY if there is none.
    Entity find_player(Imago::ECS::Nexus& nexus)
    {
        for (Entity e : nexus.survey<Player, Stats>()) {
            return e;
        }
        return Imago::ECS::NULL_ENTITY;
    }

    /// @brief Event subscriber: adds the coin's value to the score and destroys the coin.
    void on_coin_collected(SceneContext& ctx, const CoinCollected& event)
    {
        Entity player = find_player(*ctx.nexus);
        if (player == Imago::ECS::NULL_ENTITY) {
            return;
        }

        Stats& stats = ctx.nexus->get<Stats>(player);
        stats.score += event.value;
        stats.coinsLeft--;

        // destroy is deferred: the coin disappears when the frame's flush runs
        ctx.nexus->destroy(event.coin);

        spdlog::info("[Play] CoinCollected (+{}) -> score {}, {} coin(s) left", event.value,
                     stats.score, stats.coinsLeft);
        if (stats.coinsLeft == 0) {
            spdlog::info("[Play] every coin collected! R / Y restarts the scene");
        }
    }

    /// @brief Update system: reads movement and sprint input and writes the player's Velocity.
    void player_input_system(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;
        Entity player = find_player(*ctx.nexus);
        if (player == Imago::ECS::NULL_ENTITY) {
            return;
        }

        // direction() combines four actions and clamps diagonals to length 1
        const glm::vec2 dir = input.direction("move_left", "move_right", "move_up", "move_down");

        // strength() is analog: 0-1 on the trigger, 0 or 1 on the Shift key
        const float sprint = input.strength("sprint");

        // patch() overwrites a component immediately (it is not a structural change)
        ctx.nexus->patch<Velocity>(player, Velocity{dir * PLAYER_SPEED * (1.0f + sprint)});
    }

    /// @brief Update system: per-frame jump edges and held state.
    void jump_system(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        // resolving a name once and caching the id is the fast path for hot systems
        static const ActionId jump = input.action_id("jump");

        if (input.began(jump)) {
            Entity player = find_player(*ctx.nexus);
            if (player != Imago::ECS::NULL_ENTITY) {
                Stats& stats = ctx.nexus->get<Stats>(player);
                stats.jumps++;
                spdlog::info("[frame] jump began (jump #{})", stats.jumps);
            }
        }
        if (input.ended(jump)) {
            spdlog::info("[frame] jump ended");
        }
    }

    /// @brief Fixed system: the same press reaches a fixed system exactly once as well.
    void fixed_jump_log(SceneContext& ctx, float)
    {
        if (ctx.input->began("jump")) {
            spdlog::info("[fixed] jump began");
        }
        if (ctx.input->ended("jump")) {
            spdlog::info("[fixed] jump ended");
        }
    }

    /// @brief Fixed system: integrates Position from Velocity over the bond, with the fixed dt.
    void movement_system(SceneContext& ctx, float dt)
    {
        Imago::ECS::Nexus& nexus = *ctx.nexus;

        // the bond keeps every entity with Position AND Velocity packed at the front of both pools
        for (Entity e : nexus.bond<Position, Velocity>()) {
            glm::vec2& pos = nexus.get<Position>(e).value;
            pos += nexus.get<Velocity>(e).value * dt;
            pos.x = std::fmin(std::fmax(pos.x, 0.0f), WORLD_WIDTH);
            pos.y = std::fmin(std::fmax(pos.y, 0.0f), WORLD_HEIGHT);
        }
    }

    /// @brief Update system: surveys the coins and emits CoinCollected for the ones touched.
    void coin_system(SceneContext& ctx, float)
    {
        Imago::ECS::Nexus& nexus = *ctx.nexus;
        Entity player = find_player(nexus);
        if (player == Imago::ECS::NULL_ENTITY) {
            return;
        }
        const glm::vec2 playerPos = nexus.get<Position>(player).value;

        for (Entity coin : nexus.survey<Position, Coin>()) {
            if (glm::length(nexus.get<Position>(coin).value - playerPos) <= PICKUP_RADIUS) {
                ctx.broadcaster->emit(CoinCollected{coin, nexus.get<Coin>(coin).value});
            }
        }
    }

    /// @brief Update system: shows the live state in the window title.
    void play_title_system(SceneContext& ctx, float dt)
    {
        static float timer = TITLE_INTERVAL;
        if (!title_due(timer, dt)) {
            return;
        }

        InputState& input = *ctx.input;
        Imago::ECS::Nexus& nexus = *ctx.nexus;
        Entity player = find_player(nexus);
        if (player == Imago::ECS::NULL_ENTITY) {
            return;
        }

        const glm::vec2 pos = nexus.get<Position>(player).value;
        const Stats& stats = nexus.get<Stats>(player);
        const glm::vec2 dir = input.direction("move_left", "move_right", "move_up", "move_down");

        ctx.window->setTitle(fmt::format(
            "PLAY #{} | pos ({:.0f}, {:.0f}) | score {} ({} left) | dir ({:.2f}, {:.2f}) "
            "axis {:+.2f} | sprint {:.2f} | jump {} x{}",
            gPlayLoads, pos.x, pos.y, stats.score, stats.coinsLeft, dir.x, dir.y,
            input.axis("move_left", "move_right"), input.strength("sprint"),
            input.held("jump") ? "HELD" : "up", stats.jumps));
    }

    /// @brief Update system: scene switching and quit requests. All of them apply at the end of
    /// the frame, so the rest of this frame still runs.
    void play_control_system(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        if (input.began("pause")) {
            spdlog::info("[Play] pause -> switch_scene(\"Pause\", true): Play is suspended");
            ctx.engine->switch_scene("Pause", true);
        }
        if (input.began("restart")) {
            spdlog::info("[Play] restart -> switch_scene(\"Play\"): the active scene is rebuilt");
            ctx.engine->switch_scene("Play");
        }
        if (input.began("menu")) {
            spdlog::info("[Play] menu -> switch_scene(\"Menu\"): Play is destroyed");
            ctx.engine->switch_scene("Menu");
        }
        if (input.began("quit")) {
            // a quit in the same frame as a switch wins and the switch is dropped
            spdlog::info("[Play] quit began -> quit()");
            ctx.engine->quit();
        }
    }

    class PlayScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            ++gPlayLoads;
            spdlog::info("[Play] on_load (load #{}): fresh player and coins", gPlayLoads);

            // create the bond up front, before any components exist
            _nexus.bond<Position, Velocity>();

            // the player: bind is deferred, load() flushes right after on_load returns
            const glm::vec2 center(WORLD_WIDTH * 0.5f, WORLD_HEIGHT * 0.5f);
            Entity player = _nexus.create();
            _nexus.bind<Position>(player, Position{center});
            _nexus.bind<Velocity>(player, Velocity{});
            _nexus.bind<Player>(player, Player{});

            // a few coins worth different amounts
            const glm::vec2 coinSpots[] = {
                {100.0f, 100.0f}, {860.0f, 100.0f}, {100.0f, 440.0f},
                {860.0f, 440.0f}, {480.0f, 80.0f},
            };
            int value = 1;
            for (const glm::vec2& spot : coinSpots) {
                Entity coin = _nexus.create();
                _nexus.bind<Position>(coin, Position{spot});
                _nexus.bind<Coin>(coin, Coin{value++});
            }
            _nexus.bind<Stats>(player, Stats{0, 0, static_cast<int>(std::size(coinSpots))});

            // events and systems
            _broadcaster.subscribe<CoinCollected>(on_coin_collected);

            _scheduler.register_fixed(movement_system);
            _scheduler.register_fixed(fixed_jump_log);

            _scheduler.register_update(player_input_system);
            _scheduler.register_update(jump_system);
            _scheduler.register_update(coin_system);
            _scheduler.register_update(play_title_system);
            _scheduler.register_update(play_control_system);

            spdlog::info("[Play] collect the 5 coins at the corners and top-center "
                         "(watch pos in the title)");
            spdlog::info("[Play] P/Back pause (persist), R/Y restart, M/X menu, Esc/B quit");
        }

        void on_unload() override { spdlog::info("[Play] on_unload: player and coins go away"); }
    };

    // ------------------------------------------------------------------
    // Pause scene
    // ------------------------------------------------------------------

    void pause_system(SceneContext& ctx, float dt)
    {
        static float timer = TITLE_INTERVAL;
        if (title_due(timer, dt)) {
            ctx.window->setTitle(
                fmt::format("PAUSED (load #{}) | P/Back or Enter/Start: resume Play   Esc/B: quit",
                            gPauseLoads));
        }

        if (ctx.input->began("pause") || ctx.input->began("confirm")) {
            // Pause does not persist, Play was persisted so it resumes without reloading
            spdlog::info("[Pause] resume -> switch_scene(\"Play\"): the suspended Play resumes");
            ctx.engine->switch_scene("Play");
        }
        if (ctx.input->began("quit")) {
            spdlog::info("[Pause] quit began -> quit()");
            ctx.engine->quit();
        }
    }

    class PauseScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            ++gPauseLoads;
            spdlog::info("[Pause] on_load (load #{}): Play is suspended with its state kept",
                         gPauseLoads);
            spdlog::info("[Pause] P/Back or Enter/Start resumes, Esc/B quits");
            _scheduler.register_update(pause_system);
        }

        void on_unload() override { spdlog::info("[Pause] on_unload"); }
    };

    // ------------------------------------------------------------------
    // Input setup
    // ------------------------------------------------------------------

    void register_inputs(Game& game)
    {
        // movement: keyboard keys, left stick directions (default dead zone/threshold), and D-pad
        game.register_action("move_left");
        game.add_input("move_left", Scan::A);
        game.add_input("move_left", Scan::Left);
        game.add_input("move_left", GamepadAxis::LeftStickLeft);
        game.add_input("move_left", GamepadButton::DPadLeft);

        game.register_action("move_right");
        game.add_input("move_right", Scan::D);
        game.add_input("move_right", Scan::Right);
        game.add_input("move_right", GamepadAxis::LeftStickRight);
        game.add_input("move_right", GamepadButton::DPadRight);

        game.register_action("move_up");
        game.add_input("move_up", Scan::W);
        game.add_input("move_up", Scan::Up);
        game.add_input("move_up", GamepadAxis::LeftStickUp);
        game.add_input("move_up", GamepadButton::DPadUp);

        game.register_action("move_down");
        game.add_input("move_down", Scan::S);
        game.add_input("move_down", Scan::Down);
        game.add_input("move_down", GamepadAxis::LeftStickDown);
        game.add_input("move_down", GamepadButton::DPadDown);

        // sprint: a small trigger dead zone so a light pull already gives some strength
        game.register_action("sprint");
        game.add_input("sprint", Scan::LShift);
        game.add_input("sprint", GamepadAxis::RightTrigger, 0.05f, 0.3f);

        game.register_action("jump");
        game.add_input("jump", Scan::Space);
        game.add_input("jump", GamepadButton::FaceDown);

        game.register_action("confirm");
        game.add_input("confirm", Scan::Enter);
        game.add_input("confirm", GamepadButton::Start);

        game.register_action("pause");
        game.add_input("pause", Scan::P);
        game.add_input("pause", GamepadButton::Back);

        game.register_action("restart");
        game.add_input("restart", Scan::R);
        game.add_input("restart", GamepadButton::FaceUp);

        game.register_action("menu");
        game.add_input("menu", Scan::M);
        game.add_input("menu", GamepadButton::FaceLeft);

        game.register_action("quit");
        game.add_input("quit", Scan::Escape);
        game.add_input("quit", GamepadButton::FaceRight);
    }
}

int main()
{
    GameConfig config;
    config.title = "ImagoEngine demo";
    config.width = static_cast<unsigned int>(WORLD_WIDTH);
    config.height = static_cast<unsigned int>(WORLD_HEIGHT);
    config.pacing = FramePacing::VSync;
    config.firstScene = "Menu";

    Game game(config);

    game.register_scene<MenuScene>("Menu");
    game.register_scene<PlayScene>("Play");
    game.register_scene<PauseScene>("Pause");

    register_inputs(game);

    print_controls();

    game.run();

    spdlog::info("Demo finished: Menu loaded {}x, Play loaded {}x, Pause loaded {}x", gMenuLoads,
                 gPlayLoads, gPauseLoads);
    return 0;
}
