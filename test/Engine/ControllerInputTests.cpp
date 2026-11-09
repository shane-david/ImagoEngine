/**
 * @file GamepadInputTests.cpp
 * @author Shane David
 * @brief Headless tests for the gamepad path through InputHandler and InputState.
 *
 * These drive handle_gamepad_button and handle_gamepad_axis directly, which is what Game
 * calls after translating SDL events. Nothing here needs SDL, a window, or a controller.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <SFML/Window/Event.hpp>
#include <glm/vec2.hpp>

#include "Imago/Engine/InputSource.hpp"
#include "Imago/Engine/InputState.hpp"
#include "Imago/Engine/InputHandler.hpp"

#include <iostream>
#include <iomanip>

#include "Imago/Engine/Game.hpp"
#include "Imago/Engine/GameConfig.hpp"
#include "Imago/Engine/Scene.hpp"
#include "Imago/Engine/SceneContext.hpp"
#include "Imago/Engine/SystemScheduler.hpp"

using namespace Imago::Engine;
using Catch::Approx;
using Scan = sf::Keyboard::Scancode;


namespace
{
    // NOTE: if this differs from the event helper already in InputTests.cpp, use that shape.
    sf::Event make_key_event(sf::Keyboard::Scancode scancode, bool pressed)
    {
        if (pressed) {
            sf::Event::KeyPressed pressedEvent;
            pressedEvent.scancode = scancode;
            return sf::Event(pressedEvent);
        }

        sf::Event::KeyReleased releasedEvent;
        releasedEvent.scancode = scancode;
        return sf::Event(releasedEvent);
    }
}

// --- gamepad buttons ---

TEST_CASE("a gamepad button drives an action", "[gamepad][button]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("jump");
    handler.add_input("jump", GamepadButton::FaceDown);

    SECTION("press sets held, began, and full strength")
    {
        handler.handle_gamepad_button(GamepadButton::FaceDown, true);

        CHECK(state.held("jump"));
        CHECK(state.began("jump"));
        CHECK_FALSE(state.ended("jump"));
        CHECK(state.strength("jump") == Approx(1.0f));
    }

    SECTION("release clears held and sets ended")
    {
        handler.handle_gamepad_button(GamepadButton::FaceDown, true);
        handler.clear_edges();

        handler.handle_gamepad_button(GamepadButton::FaceDown, false);

        CHECK_FALSE(state.held("jump"));
        CHECK(state.ended("jump"));
        CHECK(state.strength("jump") == Approx(0.0f));
    }

    SECTION("a button that was never added does nothing")
    {
        handler.handle_gamepad_button(GamepadButton::FaceUp, true);

        CHECK_FALSE(state.held("jump"));
        CHECK(state.strength("jump") == Approx(0.0f));
    }

    SECTION("a repeated press does not re-fire began")
    {
        handler.handle_gamepad_button(GamepadButton::FaceDown, true);
        handler.clear_edges();
        handler.handle_gamepad_button(GamepadButton::FaceDown, true);

        CHECK(state.held("jump"));
        CHECK_FALSE(state.began("jump"));
    }
}

// --- the strength / held split ---

TEST_CASE("an axis below its threshold reports strength but not held", "[gamepad][axis]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);   // dead zone 0.2, threshold 0.5

    // raw 0.4 rescales to (0.4 - 0.2) / 0.8 = 0.25, under the 0.5 threshold
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);

    CHECK(state.strength("move_left") == Approx(0.25f));
    CHECK_FALSE(state.held("move_left"));
    CHECK_FALSE(state.began("move_left"));
}

TEST_CASE("an axis past its threshold reports held", "[gamepad][axis]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);

    // raw 0.8 rescales to 0.6 / 0.8 = 0.75
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);

    CHECK(state.strength("move_left") == Approx(0.75f));
    CHECK(state.held("move_left"));
    CHECK(state.began("move_left"));
}

// --- dead zone ---

TEST_CASE("the dead zone rescales usable travel back to 0-1", "[gamepad][deadzone]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);   // dead zone 0.2

    auto [raw, expected] = GENERATE(table<float, float>({
        { 0.00f, 0.00f },   // at rest
        { 0.10f, 0.00f },   // inside the dead zone
        { 0.20f, 0.00f },   // exactly at the edge
        { 0.40f, 0.25f },
        { 0.60f, 0.50f },
        { 0.80f, 0.75f },
        { 1.00f, 1.00f }    // full deflection still reaches 1
    }));

    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, raw);

    CHECK(state.strength("move_left") == Approx(expected));
}

TEST_CASE("a tighter dead zone registers smaller pushes", "[gamepad][deadzone]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("aim_right");
    handler.add_input("aim_right", GamepadAxis::RightStickRight, 0.05f, 0.5f);

    // raw 0.1 rescales to (0.1 - 0.05) / 0.95; the default 0.2 dead zone would give exactly 0
    handler.handle_gamepad_axis(GamepadAxis::RightStickRight, 0.1f);

    CHECK(state.strength("aim_right") == Approx(0.052632f).margin(0.0001f));
    CHECK_FALSE(state.held("aim_right"));
}

// --- hysteresis ---

TEST_CASE("hysteresis holds the digital state inside the band", "[gamepad][hysteresis]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);   // threshold 0.5, release at 0.4

    // raw 0.56 rescales to (0.56 - 0.2) / 0.8 = 0.45, which sits inside the 0.4..0.5 band
    const float inBand = 0.56f;

    SECTION("a value in the band does not press from rest")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, inBand);

        CHECK(state.strength("move_left") == Approx(0.45f));
        CHECK_FALSE(state.held("move_left"));
    }

    SECTION("the same value in the band does not release once down")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.9f);   // 0.875, presses
        REQUIRE(state.held("move_left"));
        handler.clear_edges();

        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, inBand);

        CHECK(state.strength("move_left") == Approx(0.45f));
        CHECK(state.held("move_left"));
        CHECK_FALSE(state.ended("move_left"));
    }

    SECTION("falling below the band releases")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.9f);
        REQUIRE(state.held("move_left"));
        handler.clear_edges();

        // raw 0.5 rescales to 0.375, under the 0.4 release point
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.5f);

        CHECK_FALSE(state.held("move_left"));
        CHECK(state.ended("move_left"));
    }
}

// --- the fan-out Game performs ---

TEST_CASE("both directions of an axis update from one stick position", "[gamepad][axis]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.register_action("move_right");
    handler.add_input("move_left",  GamepadAxis::LeftStickLeft);
    handler.add_input("move_right", GamepadAxis::LeftStickRight);

    // NOTE: this is exactly what Game does for one SDL axis event - both directions,
    // one of them zero. Skipping the zero call is what would leave an input stuck.

    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft,  0.8f);
    handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.0f);

    REQUIRE(state.held("move_left"));
    REQUIRE_FALSE(state.held("move_right"));
    handler.clear_edges();

    // stick swung across center in a single event
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft,  0.0f);
    handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.8f);

    CHECK_FALSE(state.held("move_left"));
    CHECK(state.ended("move_left"));
    CHECK(state.strength("move_left") == Approx(0.0f));

    CHECK(state.held("move_right"));
    CHECK(state.began("move_right"));
    CHECK(state.strength("move_right") == Approx(0.75f));
}

TEST_CASE("an unchanged axis reading is a no-op", "[gamepad][axis]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);

    SECTION("the same reading twice fires no second edge")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
        REQUIRE(state.began("move_left"));
        handler.clear_edges();

        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);

        CHECK(state.held("move_left"));
        CHECK_FALSE(state.began("move_left"));
        CHECK(state.strength("move_left") == Approx(0.75f));
    }

    SECTION("two different readings inside the dead zone both collapse to zero")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.10f);
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.15f);

        CHECK(state.strength("move_left") == Approx(0.0f));
        CHECK_FALSE(state.began("move_left"));
    }
}

// --- the two reductions in refresh_action ---

TEST_CASE("a dpad and a stick direction on one action reduce independently", "[gamepad][mixed]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadButton::DPadLeft);
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);

    SECTION("a light stick push gives analog strength without held")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);

        CHECK(state.strength("move_left") == Approx(0.25f));
        CHECK_FALSE(state.held("move_left"));
    }

    SECTION("the dpad wins the strength max and sets held")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);
        handler.handle_gamepad_button(GamepadButton::DPadLeft, true);

        CHECK(state.strength("move_left") == Approx(1.0f));
        CHECK(state.held("move_left"));
        CHECK(state.began("move_left"));
    }

    SECTION("releasing the dpad falls back to the stick's analog value")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);
        handler.handle_gamepad_button(GamepadButton::DPadLeft, true);
        handler.clear_edges();

        handler.handle_gamepad_button(GamepadButton::DPadLeft, false);

        CHECK(state.strength("move_left") == Approx(0.25f));
        CHECK_FALSE(state.held("move_left"));
        CHECK(state.ended("move_left"));
    }
}

TEST_CASE("two actions on one trigger with different thresholds", "[gamepad][trigger]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("aim_down_sights");
    handler.add_input("aim_down_sights", GamepadAxis::LeftTrigger, 0.05f, 0.2f);

    handler.register_action("fire");
    handler.add_input("fire", GamepadAxis::LeftTrigger, 0.05f, 0.8f);

    // NOTE: this is the case that proves held cannot be derived from strength. Both actions
    // see the same analog value and disagree about whether it counts as a press.

    SECTION("a light pull aims but does not fire")
    {
        // raw 0.5 rescales to (0.5 - 0.05) / 0.95 = 0.4737
        handler.handle_gamepad_axis(GamepadAxis::LeftTrigger, 0.5f);

        CHECK(state.held("aim_down_sights"));
        CHECK_FALSE(state.held("fire"));
        CHECK(state.strength("fire") == Approx(0.473684f).margin(0.0001f));
    }

    SECTION("a full pull does both")
    {
        // raw 0.95 rescales to 0.9 / 0.95 = 0.9474
        handler.handle_gamepad_axis(GamepadAxis::LeftTrigger, 0.95f);

        CHECK(state.held("aim_down_sights"));
        CHECK(state.held("fire"));
    }
}

// --- disconnect ---

TEST_CASE("release_gamepad_inputs clears only gamepad entries", "[gamepad][disconnect]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", sf::Keyboard::Scancode::A);
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);

    handler.register_action("jump");
    handler.add_input("jump", GamepadButton::FaceDown);

    handler.handle_event(make_key_event(sf::Keyboard::Scancode::A, true));
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
    handler.handle_gamepad_button(GamepadButton::FaceDown, true);

    REQUIRE(state.held("move_left"));
    REQUIRE(state.held("jump"));
    handler.clear_edges();

    handler.release_gamepad_inputs();

    SECTION("a held key keeps the mixed action held")
    {
        CHECK(state.held("move_left"));
        CHECK(state.strength("move_left") == Approx(1.0f));
        CHECK_FALSE(state.ended("move_left"));
    }

    SECTION("a gamepad-only action releases and fires ended")
    {
        CHECK_FALSE(state.held("jump"));
        CHECK(state.strength("jump") == Approx(0.0f));
        CHECK(state.ended("jump"));
    }
}

// --- tuning replacement ---

TEST_CASE("re-registering an input replaces its tuning", "[gamepad][tuning]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);   // 0.2, 0.5

    SECTION("the new tuning takes effect")
    {
        handler.add_input("move_left", GamepadAxis::LeftStickLeft, 0.0f, 0.1f);

        // under the original tuning this would rescale to exactly 0 and never press
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.2f);

        CHECK(state.strength("move_left") == Approx(0.2f));
        CHECK(state.held("move_left"));
    }

    SECTION("raising the threshold does not leave a held input stuck")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
        REQUIRE(state.held("move_left"));
        handler.clear_edges();

        handler.add_input("move_left", GamepadAxis::LeftStickLeft, 0.2f, 0.9f);

        CHECK_FALSE(state.held("move_left"));
        CHECK(state.strength("move_left") == Approx(0.0f));
        CHECK(state.ended("move_left"));
    }

    SECTION("identical tuning is a silent duplicate and does not reset")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
        REQUIRE(state.held("move_left"));
        handler.clear_edges();

        handler.add_input("move_left", GamepadAxis::LeftStickLeft, 0.2f, 0.5f);

        CHECK(state.held("move_left"));
        CHECK(state.strength("move_left") == Approx(0.75f));
        CHECK_FALSE(state.ended("move_left"));
    }
}

// --- queries ---

TEST_CASE("axis and direction work with gamepad inputs", "[gamepad][queries]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.register_action("move_right");
    handler.register_action("move_up");
    handler.register_action("move_down");

    handler.add_input("move_left",  GamepadAxis::LeftStickLeft);
    handler.add_input("move_right", GamepadAxis::LeftStickRight);
    handler.add_input("move_up",    GamepadAxis::LeftStickUp);
    handler.add_input("move_down",  GamepadAxis::LeftStickDown);

    SECTION("axis returns positive minus negative")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.8f);   // 0.75
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft,  0.0f);

        CHECK(state.axis("move_left", "move_right") == Approx(0.75f));
    }

    SECTION("a diagonal under length 1 is not clamped")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.6f);   // 0.5
        handler.handle_gamepad_axis(GamepadAxis::LeftStickDown,  0.6f);   // 0.5

        const glm::vec2 dir = state.direction("move_left", "move_right", "move_up", "move_down");

        CHECK(dir.x == Approx(0.5f));
        CHECK(dir.y == Approx(0.5f));
    }
}

// --- Unknown ---

TEST_CASE("an Unknown from the event path matches nothing", "[gamepad][unknown]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("jump");
    handler.add_input("jump", GamepadButton::FaceDown);

    // from_sdl returns Unknown for buttons Imago does not model, such as paddles or a touchpad
    handler.handle_gamepad_button(GamepadButton::Unknown, true);
    handler.handle_gamepad_axis(GamepadAxis::Unknown, 1.0f);

    CHECK_FALSE(state.held("jump"));
    CHECK(state.strength("jump") == Approx(0.0f));
}

#ifdef NDEBUG
TEST_CASE("Unknown is rejected at registration in Release", "[gamepad][unknown]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("jump");
    handler.add_input("jump", GamepadButton::Unknown);
    handler.add_input("jump", GamepadAxis::Unknown);

    handler.handle_gamepad_button(GamepadButton::Unknown, true);
    handler.handle_gamepad_axis(GamepadAxis::Unknown, 1.0f);

    CHECK_FALSE(state.held("jump"));
    CHECK(state.strength("jump") == Approx(0.0f));
}
#endif

// ---------------------------------------------------------------------------
// Interactive check (needs a real window and a controller)
// run the test executable with "[gamepad-interactive]"
// ---------------------------------------------------------------------------

namespace
{
    const char* const GAMEPAD_ACTIONS[] = {
        "gp_jump", "gp_aim", "gp_fire", "gp_dash",
        "gp_left", "gp_right", "gp_up", "gp_down"
    };

    void GamepadFrameSystem(SceneContext& ctx, float dt)
    {
        InputState& input = *ctx.input;

        if (input.began("gp_quit")) {
            std::cout << "Start pressed, quitting" << std::endl;
            ctx.engine->quit();
            return;
        }

        // discrete events print every time they happen
        for (const char* name : GAMEPAD_ACTIONS) {
            if (input.began(name)) std::cout << "[frame] began  " << name << std::endl;
            if (input.ended(name)) std::cout << "[frame] ended  " << name << std::endl;
        }

        // NOTE: analog values change on almost every event, so throttle the readout or the
        // console is unreadable. Four lines a second is enough to watch the numbers move.
        static float elapsed = 0.0f;
        elapsed += dt;
        if (elapsed < 0.25f) return;
        elapsed = 0.0f;

        const glm::vec2 dir = input.direction("gp_left", "gp_right", "gp_up", "gp_down");
        const float leftStrength = input.strength("gp_left");
        const float triggerStrength = input.strength("gp_fire");

        // stay quiet while everything is at rest
        if (dir == glm::vec2(0.0f) && triggerStrength == 0.0f) return;

        std::cout << std::fixed << std::setprecision(3)
                  << "  stick (" << dir.x << ", " << dir.y << ")"
                  << "   left  strength " << leftStrength
                  << "  held " << input.held("gp_left")
                  << "  dash " << input.held("gp_dash")
                  << "   |  trigger strength " << triggerStrength
                  << "  aim " << input.held("gp_aim")
                  << "  fire " << input.held("gp_fire")
                  << std::endl;
    }

    // a press should reach a fixed system exactly once as well
    void GamepadFixedSystem(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        for (const char* name : GAMEPAD_ACTIONS) {
            if (input.began(name)) std::cout << "[fixed] began  " << name << std::endl;
            if (input.ended(name)) std::cout << "[fixed] ended  " << name << std::endl;
        }
    }

    class GamepadDemoScene : public Scene {
    public:
        using Scene::Scene;
    protected:
        void on_load() override
        {
            _scheduler.register_update(GamepadFrameSystem);
            _scheduler.register_fixed(GamepadFixedSystem);
        }
    };
}

TEST_CASE("Interactive gamepad check", "[.gamepad-interactive]")
{
    GameConfig config;
    config.title = "ImagoEngine gamepad check (Start or Esc quits)";
    config.width = 800;
    config.height = 450;
    config.pacing = FramePacing::VSync;
    config.firstScene = "GamepadDemo";

    Game game(config);

    game.register_action("gp_jump");
    game.add_input("gp_jump", GamepadButton::FaceDown);

    // two actions on ONE trigger with different thresholds: a light pull aims, a full pull fires
    game.register_action("gp_aim");
    game.add_input("gp_aim", GamepadAxis::RightTrigger, 0.05f, 0.2f);

    game.register_action("gp_fire");
    game.add_input("gp_fire", GamepadAxis::RightTrigger, 0.05f, 0.8f);

    // left stick directions on the defaults
    game.register_action("gp_left");
    game.add_input("gp_left", GamepadAxis::LeftStickLeft);

    game.register_action("gp_right");
    game.add_input("gp_right", GamepadAxis::LeftStickRight);

    game.register_action("gp_up");
    game.add_input("gp_up", GamepadAxis::LeftStickUp);

    game.register_action("gp_down");
    game.add_input("gp_down", GamepadAxis::LeftStickDown);

    // the same stick direction as gp_left but demanding a decisive push
    game.register_action("gp_dash");
    game.add_input("gp_dash", GamepadAxis::LeftStickLeft, 0.2f, 0.85f);

    // Start quits from the pad, Escape quits if no controller shows up at all
    game.register_action("gp_quit");
    game.add_input("gp_quit", GamepadButton::Start);
    game.add_input("gp_quit", Scan::Escape);

    game.register_scene<GamepadDemoScene>("GamepadDemo");

    std::cout << "\n=== Gamepad check ===\n"
              << "A 'controller connected' line should appear above if one is plugged in.\n"
              << "Click the window to focus it, then try:\n"
              << "  Bottom face button    gp_jump (A on Xbox, Cross on PlayStation)\n"
              << "  Right trigger         pull lightly -> aim 1, fire 0; pull fully -> both 1\n"
              << "  Left stick, gently    strength rises while held stays 0  <- the main thing to watch\n"
              << "  Left stick, harder    held goes to 1, dash stays 0\n"
              << "  Left stick, fully     dash goes to 1 as well\n"
              << "  Start or Esc          quit\n"
              << "\nAlso worth trying:\n"
              << "  Swing the stick fast from left to right: gp_left should print 'ended'.\n"
              << "  Hold a button, then click another window: 'ended' should print, and the\n"
              << "    numbers should stop moving until you click back.\n"
              << "  Hold a button and unplug the controller: 'ended' should print immediately.\n"
              << "  Plug the controller back in: a 'controller connected' line should appear.\n"
              << std::endl;

    game.run();

    SUCCEED("interactive gamepad check finished");
}