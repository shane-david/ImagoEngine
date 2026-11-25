/**
 * @file InputHandlerTests.cpp
 * @brief Tests InputHandler, which turns raw keyboard, mouse, and gamepad input into action state,
 * plus two interactive checks through a real Game (hidden behind [.interactive]).
 *
 * The gamepad tests drive handle_gamepad_button and handle_gamepad_axis directly, which is what
 * Game calls after translating SDL events, so they need no SDL, window, or controller.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <iomanip>
#include <iostream>

#include <glm/vec2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include "Imago/Engine/EngineControl.hpp"
#include "Imago/Engine/Game.hpp"
#include "Imago/Engine/GameConfig.hpp"
#include "Imago/Engine/InputHandler.hpp"
#include "Imago/Engine/InputSource.hpp"
#include "Imago/Engine/InputState.hpp"
#include "Imago/Engine/Scene.hpp"
#include "Imago/Engine/SceneContext.hpp"
#include "Imago/Engine/SystemScheduler.hpp"

using namespace Imago::Engine;

namespace
{
    using Scan = sf::Keyboard::Scancode;

    // --- hand-built SFML events, the event structs are plain aggregates ---

    sf::Event key_pressed(Scan scancode)
    {
        sf::Event::KeyPressed e;
        e.scancode = scancode;
        return sf::Event(e);
    }

    sf::Event key_released(Scan scancode)
    {
        sf::Event::KeyReleased e;
        e.scancode = scancode;
        return sf::Event(e);
    }

    sf::Event mouse_pressed(sf::Mouse::Button button)
    {
        sf::Event::MouseButtonPressed e;
        e.button = button;
        return sf::Event(e);
    }

    sf::Event mouse_released(sf::Mouse::Button button)
    {
        sf::Event::MouseButtonReleased e;
        e.button = button;
        return sf::Event(e);
    }

    sf::Event focus_lost()
    {
        return sf::Event(sf::Event::FocusLost{});
    }
}

// ---------------------------------------------------------------------------
// Keyboard and mouse
// ---------------------------------------------------------------------------

TEST_CASE("InputHandler register_action returns the same id for a duplicate", "[InputHandler]")
{
    InputHandler handler;

    const ActionId first = handler.register_action("jump");
    const ActionId again = handler.register_action("jump");

    CHECK(first == again);
    CHECK(handler.state().action_id("jump") == first);
}

TEST_CASE("InputHandler turns key events into action state", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));

    CHECK(handler.state().held("jump"));
    CHECK(handler.state().began("jump"));
    CHECK(handler.state().strength("jump") == Catch::Approx(1.0f));

    handler.handle_event(key_released(Scan::Space));

    CHECK_FALSE(handler.state().held("jump"));
    CHECK(handler.state().ended("jump"));
    CHECK(handler.state().strength("jump") == Catch::Approx(0.0f));
}

TEST_CASE("InputHandler turns mouse button events into action state", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("fire");
    handler.add_input("fire", sf::Mouse::Button::Left);

    handler.handle_event(mouse_pressed(sf::Mouse::Button::Left));
    CHECK(handler.state().held("fire"));
    CHECK(handler.state().began("fire"));

    handler.handle_event(mouse_released(sf::Mouse::Button::Left));
    CHECK_FALSE(handler.state().held("fire"));
    CHECK(handler.state().ended("fire"));
}

TEST_CASE("InputHandler ignores a repeated KeyPressed for a held key", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));
    handler.clear_edges();

    // the OS repeats the press while the key is held
    handler.handle_event(key_pressed(Scan::Space));
    handler.handle_event(key_pressed(Scan::Space));

    CHECK(handler.state().held("jump"));
    CHECK_FALSE(handler.state().began("jump"));

    // a single release still ends it
    handler.handle_event(key_released(Scan::Space));
    CHECK_FALSE(handler.state().held("jump"));
    CHECK(handler.state().ended("jump"));
}

TEST_CASE("InputHandler keeps an action held until every input is released", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("move_left");
    handler.add_input("move_left", Scan::A);
    handler.add_input("move_left", Scan::Left);

    handler.handle_event(key_pressed(Scan::A));
    handler.handle_event(key_pressed(Scan::Left));
    handler.clear_edges();

    handler.handle_event(key_released(Scan::A));
    CHECK(handler.state().held("move_left"));
    CHECK_FALSE(handler.state().ended("move_left"));

    handler.handle_event(key_released(Scan::Left));
    CHECK_FALSE(handler.state().held("move_left"));
    CHECK(handler.state().ended("move_left"));
}

TEST_CASE("InputHandler lets one input drive several actions", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.register_action("confirm");
    handler.add_input("jump", Scan::Space);
    handler.add_input("confirm", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));

    CHECK(handler.state().held("jump"));
    CHECK(handler.state().held("confirm"));
}

TEST_CASE("InputHandler ignores inputs that are not added", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Q));
    handler.handle_event(mouse_pressed(sf::Mouse::Button::Right));

    CHECK_FALSE(handler.state().held("jump"));
    CHECK_FALSE(handler.state().began("jump"));
}

TEST_CASE("InputHandler does not confuse a key and a mouse button", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("from_key");
    handler.register_action("from_mouse");
    handler.add_input("from_key", Scan::A);
    handler.add_input("from_mouse", sf::Mouse::Button::Left);

    handler.handle_event(mouse_pressed(sf::Mouse::Button::Left));

    CHECK(handler.state().held("from_mouse"));
    CHECK_FALSE(handler.state().held("from_key"));
}

TEST_CASE("InputHandler adding the same input twice behaves like adding it once", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));
    CHECK(handler.state().held("jump"));

    handler.handle_event(key_released(Scan::Space));
    CHECK_FALSE(handler.state().held("jump"));
}

TEST_CASE("InputHandler focus loss releases everything", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.register_action("fire");
    handler.register_action("pad");
    handler.add_input("jump", Scan::Space);
    handler.add_input("fire", sf::Mouse::Button::Left);
    handler.add_input("pad", GamepadButton::FaceDown);

    handler.handle_event(key_pressed(Scan::Space));
    handler.handle_event(mouse_pressed(sf::Mouse::Button::Left));
    handler.handle_gamepad_button(GamepadButton::FaceDown, true);
    handler.clear_edges();

    handler.handle_event(focus_lost());

    CHECK_FALSE(handler.state().held("jump"));
    CHECK_FALSE(handler.state().held("fire"));
    CHECK_FALSE(handler.state().held("pad"));
    CHECK(handler.state().ended("jump"));
    CHECK(handler.state().ended("fire"));
    CHECK(handler.state().ended("pad"));

    // a key released while unfocused never sends a release event, so a fresh press must work again
    handler.handle_event(key_pressed(Scan::Space));
    CHECK(handler.state().held("jump"));
}

TEST_CASE("InputHandler clear_edges keeps held state", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));
    handler.clear_edges();

    CHECK_FALSE(handler.state().began("jump"));
    CHECK(handler.state().held("jump"));
}

TEST_CASE("InputHandler add_input by ActionId matches add_input by name", "[InputHandler]")
{
    InputHandler handler;
    const ActionId jump = handler.register_action("jump");
    handler.add_input(jump, KeySource{Scan::Space});

    handler.handle_event(key_pressed(Scan::Space));

    CHECK(handler.state().held(jump));
    CHECK(handler.state().held("jump"));
}

#ifdef NDEBUG
TEST_CASE("InputHandler ignores bad add_input calls in Release", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");

    // an Unknown key can never be added, so Unknown events match nothing
    handler.add_input("jump", Scan::Unknown);
    handler.handle_event(key_pressed(Scan::Unknown));
    CHECK_FALSE(handler.state().held("jump"));

    // an unregistered action is ignored
    handler.add_input("nope", Scan::Space);
    handler.handle_event(key_pressed(Scan::Space));
    CHECK_FALSE(handler.state().held("jump"));
}
#endif

// ---------------------------------------------------------------------------
// Gamepad buttons and axes
// ---------------------------------------------------------------------------

TEST_CASE("A gamepad button drives an action", "[InputHandler][Gamepad]")
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
        CHECK(state.strength("jump") == Catch::Approx(1.0f));
    }

    SECTION("release clears held and sets ended")
    {
        handler.handle_gamepad_button(GamepadButton::FaceDown, true);
        handler.clear_edges();

        handler.handle_gamepad_button(GamepadButton::FaceDown, false);

        CHECK_FALSE(state.held("jump"));
        CHECK(state.ended("jump"));
        CHECK(state.strength("jump") == Catch::Approx(0.0f));
    }

    SECTION("a button that was never added does nothing")
    {
        handler.handle_gamepad_button(GamepadButton::FaceUp, true);

        CHECK_FALSE(state.held("jump"));
        CHECK(state.strength("jump") == Catch::Approx(0.0f));
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

TEST_CASE("A gamepad axis below its threshold reports strength but not held",
          "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft); // dead zone 0.2, threshold 0.5

    // raw 0.4 rescales to (0.4 - 0.2) / 0.8 = 0.25, under the 0.5 threshold
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);

    CHECK(state.strength("move_left") == Catch::Approx(0.25f));
    CHECK_FALSE(state.held("move_left"));
    CHECK_FALSE(state.began("move_left"));
}

TEST_CASE("A gamepad axis past its threshold reports held", "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);

    // raw 0.8 rescales to 0.6 / 0.8 = 0.75
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);

    CHECK(state.strength("move_left") == Catch::Approx(0.75f));
    CHECK(state.held("move_left"));
    CHECK(state.began("move_left"));
}

TEST_CASE("The gamepad dead zone rescales usable travel back to 0-1", "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft); // dead zone 0.2

    auto [raw, expected] = GENERATE(table<float, float>({
        {0.00f, 0.00f}, // at rest
        {0.10f, 0.00f}, // inside the dead zone
        {0.20f, 0.00f}, // exactly at the edge
        {0.40f, 0.25f},
        {0.60f, 0.50f},
        {0.80f, 0.75f},
        {1.00f, 1.00f} // full deflection still reaches 1
    }));

    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, raw);

    CHECK(state.strength("move_left") == Catch::Approx(expected));
}

TEST_CASE("A tighter gamepad dead zone registers smaller pushes", "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("aim_right");
    handler.add_input("aim_right", GamepadAxis::RightStickRight, 0.05f, 0.5f);

    // raw 0.1 rescales to (0.1 - 0.05) / 0.95; the default 0.2 dead zone would give exactly 0
    handler.handle_gamepad_axis(GamepadAxis::RightStickRight, 0.1f);

    CHECK(state.strength("aim_right") == Catch::Approx(0.052632f).margin(0.0001f));
    CHECK_FALSE(state.held("aim_right"));
}

TEST_CASE("Gamepad axis hysteresis holds the digital state inside the band",
          "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft); // threshold 0.5, release at 0.4

    // raw 0.56 rescales to (0.56 - 0.2) / 0.8 = 0.45, which sits inside the 0.4..0.5 band
    const float inBand = 0.56f;

    SECTION("a value in the band does not press from rest")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, inBand);

        CHECK(state.strength("move_left") == Catch::Approx(0.45f));
        CHECK_FALSE(state.held("move_left"));
    }

    SECTION("the same value in the band does not release once down")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.9f); // 0.875, presses
        REQUIRE(state.held("move_left"));
        handler.clear_edges();

        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, inBand);

        CHECK(state.strength("move_left") == Catch::Approx(0.45f));
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

TEST_CASE("Both directions of a gamepad axis update from one stick position",
          "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.register_action("move_right");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);
    handler.add_input("move_right", GamepadAxis::LeftStickRight);

    // NOTE: this is exactly what Game does for one SDL axis event - both directions,
    // one of them zero. Skipping the zero call is what would leave an input stuck.

    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
    handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.0f);

    REQUIRE(state.held("move_left"));
    REQUIRE_FALSE(state.held("move_right"));
    handler.clear_edges();

    // stick swung across center in a single event
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.0f);
    handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.8f);

    CHECK_FALSE(state.held("move_left"));
    CHECK(state.ended("move_left"));
    CHECK(state.strength("move_left") == Catch::Approx(0.0f));

    CHECK(state.held("move_right"));
    CHECK(state.began("move_right"));
    CHECK(state.strength("move_right") == Catch::Approx(0.75f));
}

TEST_CASE("An unchanged gamepad axis reading is a no-op", "[InputHandler][Gamepad]")
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
        CHECK(state.strength("move_left") == Catch::Approx(0.75f));
    }

    SECTION("two different readings inside the dead zone both collapse to zero")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.10f);
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.15f);

        CHECK(state.strength("move_left") == Catch::Approx(0.0f));
        CHECK_FALSE(state.began("move_left"));
    }
}

TEST_CASE("A dpad and a stick direction on one action reduce independently",
          "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadButton::DPadLeft);
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);

    SECTION("a light stick push gives analog strength without held")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);

        CHECK(state.strength("move_left") == Catch::Approx(0.25f));
        CHECK_FALSE(state.held("move_left"));
    }

    SECTION("the dpad wins the strength max and sets held")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);
        handler.handle_gamepad_button(GamepadButton::DPadLeft, true);

        CHECK(state.strength("move_left") == Catch::Approx(1.0f));
        CHECK(state.held("move_left"));
        CHECK(state.began("move_left"));
    }

    SECTION("releasing the dpad falls back to the stick's analog value")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.4f);
        handler.handle_gamepad_button(GamepadButton::DPadLeft, true);
        handler.clear_edges();

        handler.handle_gamepad_button(GamepadButton::DPadLeft, false);

        CHECK(state.strength("move_left") == Catch::Approx(0.25f));
        CHECK_FALSE(state.held("move_left"));
        CHECK(state.ended("move_left"));
    }
}

TEST_CASE("Two actions on one trigger with different thresholds", "[InputHandler][Gamepad]")
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
        CHECK(state.strength("fire") == Catch::Approx(0.473684f).margin(0.0001f));
    }

    SECTION("a full pull does both")
    {
        // raw 0.95 rescales to 0.9 / 0.95 = 0.9474
        handler.handle_gamepad_axis(GamepadAxis::LeftTrigger, 0.95f);

        CHECK(state.held("aim_down_sights"));
        CHECK(state.held("fire"));
    }
}

TEST_CASE("release_gamepad_inputs clears only gamepad entries", "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", Scan::A);
    handler.add_input("move_left", GamepadAxis::LeftStickLeft);

    handler.register_action("jump");
    handler.add_input("jump", GamepadButton::FaceDown);

    handler.handle_event(key_pressed(Scan::A));
    handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
    handler.handle_gamepad_button(GamepadButton::FaceDown, true);

    REQUIRE(state.held("move_left"));
    REQUIRE(state.held("jump"));
    handler.clear_edges();

    handler.release_gamepad_inputs();

    SECTION("a held key keeps the mixed action held")
    {
        CHECK(state.held("move_left"));
        CHECK(state.strength("move_left") == Catch::Approx(1.0f));
        CHECK_FALSE(state.ended("move_left"));
    }

    SECTION("a gamepad-only action releases and fires ended")
    {
        CHECK_FALSE(state.held("jump"));
        CHECK(state.strength("jump") == Catch::Approx(0.0f));
        CHECK(state.ended("jump"));
    }
}

TEST_CASE("Re-adding a gamepad axis replaces its tuning", "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.add_input("move_left", GamepadAxis::LeftStickLeft); // 0.2, 0.5

    SECTION("the new tuning takes effect")
    {
        handler.add_input("move_left", GamepadAxis::LeftStickLeft, 0.0f, 0.1f);

        // under the original tuning this would rescale to exactly 0 and never press
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.2f);

        CHECK(state.strength("move_left") == Catch::Approx(0.2f));
        CHECK(state.held("move_left"));
    }

    SECTION("raising the threshold does not leave a held input stuck")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
        REQUIRE(state.held("move_left"));
        handler.clear_edges();

        handler.add_input("move_left", GamepadAxis::LeftStickLeft, 0.2f, 0.9f);

        CHECK_FALSE(state.held("move_left"));
        CHECK(state.strength("move_left") == Catch::Approx(0.0f));
        CHECK(state.ended("move_left"));
    }

    SECTION("identical tuning is a silent duplicate and does not reset")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.8f);
        REQUIRE(state.held("move_left"));
        handler.clear_edges();

        handler.add_input("move_left", GamepadAxis::LeftStickLeft, 0.2f, 0.5f);

        CHECK(state.held("move_left"));
        CHECK(state.strength("move_left") == Catch::Approx(0.75f));
        CHECK_FALSE(state.ended("move_left"));
    }
}

TEST_CASE("axis and direction work with gamepad inputs", "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("move_left");
    handler.register_action("move_right");
    handler.register_action("move_up");
    handler.register_action("move_down");

    handler.add_input("move_left", GamepadAxis::LeftStickLeft);
    handler.add_input("move_right", GamepadAxis::LeftStickRight);
    handler.add_input("move_up", GamepadAxis::LeftStickUp);
    handler.add_input("move_down", GamepadAxis::LeftStickDown);

    SECTION("axis returns positive minus negative")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.8f); // 0.75
        handler.handle_gamepad_axis(GamepadAxis::LeftStickLeft, 0.0f);

        CHECK(state.axis("move_left", "move_right") == Catch::Approx(0.75f));
    }

    SECTION("a diagonal under length 1 is not clamped")
    {
        handler.handle_gamepad_axis(GamepadAxis::LeftStickRight, 0.6f); // 0.5
        handler.handle_gamepad_axis(GamepadAxis::LeftStickDown, 0.6f);  // 0.5

        const glm::vec2 dir = state.direction("move_left", "move_right", "move_up", "move_down");

        CHECK(dir.x == Catch::Approx(0.5f));
        CHECK(dir.y == Catch::Approx(0.5f));
    }
}

TEST_CASE("An Unknown gamepad input from the event path matches nothing", "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("jump");
    handler.add_input("jump", GamepadButton::FaceDown);

    // from_sdl returns Unknown for buttons Imago does not model, such as paddles or a touchpad
    handler.handle_gamepad_button(GamepadButton::Unknown, true);
    handler.handle_gamepad_axis(GamepadAxis::Unknown, 1.0f);

    CHECK_FALSE(state.held("jump"));
    CHECK(state.strength("jump") == Catch::Approx(0.0f));
}

#ifdef NDEBUG
TEST_CASE("An Unknown gamepad input is rejected at registration in Release",
          "[InputHandler][Gamepad]")
{
    InputHandler handler;
    InputState& state = handler.state();

    handler.register_action("jump");
    handler.add_input("jump", GamepadButton::Unknown);
    handler.add_input("jump", GamepadAxis::Unknown);

    handler.handle_gamepad_button(GamepadButton::Unknown, true);
    handler.handle_gamepad_axis(GamepadAxis::Unknown, 1.0f);

    CHECK_FALSE(state.held("jump"));
    CHECK(state.strength("jump") == Catch::Approx(0.0f));
}
#endif

// ---------------------------------------------------------------------------
// Interactive checks (need a real window and a person, hidden behind [.interactive])
// ---------------------------------------------------------------------------

namespace
{
    // --- keyboard and mouse ---

    const char* const KEYBOARD_ACTIONS[] = {"jump",       "fire",    "alt_fire",  "move_left",
                                            "move_right", "move_up", "move_down", "quit_demo"};

    /// @brief Per-frame systems read edges as "this frame".
    void keyboard_frame_system(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        for (const char* name : KEYBOARD_ACTIONS) {
            if (input.began(name)) {
                std::cout << "[frame] began  " << name << std::endl;
            }
            if (input.ended(name)) {
                std::cout << "[frame] ended  " << name << std::endl;
            }
        }

        // print the movement direction only when it changes
        static glm::vec2 last(0.0f);
        const glm::vec2 dir = input.direction("move_left", "move_right", "move_up", "move_down");
        if (dir != last) {
            std::cout << "[frame] direction (" << dir.x << ", " << dir.y << ")" << std::endl;
            last = dir;
        }

        if (input.began("quit_demo")) {
            std::cout << "Escape pressed, quitting" << std::endl;
            ctx.engine->quit();
        }
    }

    /// @brief Fixed systems read edges as "since the last fixed step", so each press should
    /// print here exactly once too.
    void keyboard_fixed_system(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        for (const char* name : KEYBOARD_ACTIONS) {
            if (input.began(name)) {
                std::cout << "[fixed] began  " << name << std::endl;
            }
            if (input.ended(name)) {
                std::cout << "[fixed] ended  " << name << std::endl;
            }
        }
    }

    class KeyboardCheckScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            _scheduler.register_update(keyboard_frame_system);
            _scheduler.register_fixed(keyboard_fixed_system);
        }
    };

    // --- gamepad ---

    const char* const GAMEPAD_ACTIONS[] = {"gp_jump", "gp_aim",   "gp_fire", "gp_dash",
                                           "gp_left", "gp_right", "gp_up",   "gp_down"};

    void gamepad_frame_system(SceneContext& ctx, float dt)
    {
        InputState& input = *ctx.input;

        if (input.began("gp_quit")) {
            std::cout << "Start pressed, quitting" << std::endl;
            ctx.engine->quit();
            return;
        }

        // discrete events print every time they happen
        for (const char* name : GAMEPAD_ACTIONS) {
            if (input.began(name)) {
                std::cout << "[frame] began  " << name << std::endl;
            }
            if (input.ended(name)) {
                std::cout << "[frame] ended  " << name << std::endl;
            }
        }

        // NOTE: analog values change on almost every event, so throttle the readout or the
        // console is unreadable. Four lines a second is enough to watch the numbers move.
        static float elapsed = 0.0f;
        elapsed += dt;
        if (elapsed < 0.25f) {
            return;
        }
        elapsed = 0.0f;

        const glm::vec2 dir = input.direction("gp_left", "gp_right", "gp_up", "gp_down");
        const float leftStrength = input.strength("gp_left");
        const float triggerStrength = input.strength("gp_fire");

        // stay quiet while everything is at rest
        if (dir == glm::vec2(0.0f) && triggerStrength == 0.0f) {
            return;
        }

        std::cout << std::fixed << std::setprecision(3) << "  stick (" << dir.x << ", " << dir.y
                  << ")"
                  << "   left  strength " << leftStrength << "  held " << input.held("gp_left")
                  << "  dash " << input.held("gp_dash") << "   |  trigger strength "
                  << triggerStrength << "  aim " << input.held("gp_aim") << "  fire "
                  << input.held("gp_fire") << std::endl;
    }

    /// @brief A press should reach a fixed system exactly once as well.
    void gamepad_fixed_system(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        for (const char* name : GAMEPAD_ACTIONS) {
            if (input.began(name)) {
                std::cout << "[fixed] began  " << name << std::endl;
            }
            if (input.ended(name)) {
                std::cout << "[fixed] ended  " << name << std::endl;
            }
        }
    }

    class GamepadCheckScene : public Scene {
    public:
        using Scene::Scene;

    protected:
        void on_load() override
        {
            _scheduler.register_update(gamepad_frame_system);
            _scheduler.register_fixed(gamepad_fixed_system);
        }
    };
}

TEST_CASE("Interactive keyboard and mouse input check", "[InputHandler][.interactive]")
{
    GameConfig config;
    config.title = "ImagoEngine input check (Esc quits)";
    config.width = 800;
    config.height = 450;
    config.pacing = FramePacing::VSync;
    config.firstScene = "KeyboardCheck";

    Game game(config);

    // W drives both jump and move_up on purpose, to check that one input can drive two actions
    game.register_action("jump");
    game.add_input("jump", Scan::Space);
    game.add_input("jump", Scan::W);

    game.register_action("fire");
    game.add_input("fire", sf::Mouse::Button::Left);

    game.register_action("alt_fire");
    game.add_input("alt_fire", sf::Mouse::Button::Right);

    game.register_action("move_left");
    game.add_input("move_left", Scan::A);
    game.add_input("move_left", Scan::Left);

    game.register_action("move_right");
    game.add_input("move_right", Scan::D);
    game.add_input("move_right", Scan::Right);

    game.register_action("move_up");
    game.add_input("move_up", Scan::W);
    game.add_input("move_up", Scan::Up);

    game.register_action("move_down");
    game.add_input("move_down", Scan::S);
    game.add_input("move_down", Scan::Down);

    game.register_action("quit_demo");
    game.add_input("quit_demo", Scan::Escape);

    game.register_scene<KeyboardCheckScene>("KeyboardCheck");

    std::cout << "\n=== Input check ===\n"
              << "Click the window to focus it, then try:\n"
              << "  Space or W        jump (W is also move_up)\n"
              << "  A/D/W/S or arrows move_left / move_right / move_up / move_down\n"
              << "  Left / Right mouse button   fire / alt_fire\n"
              << "  Esc               quit\n"
              << "Each press should print once under [frame] and once under [fixed].\n"
              << "Hold a key while you click another window, its 'ended' should print.\n"
              << std::endl;

    game.run();

    SUCCEED("interactive check finished");
}

TEST_CASE("Interactive gamepad input check", "[InputHandler][Gamepad][.interactive]")
{
    GameConfig config;
    config.title = "ImagoEngine gamepad check (Start or Esc quits)";
    config.width = 800;
    config.height = 450;
    config.pacing = FramePacing::VSync;
    config.firstScene = "GamepadCheck";

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

    game.register_scene<GamepadCheckScene>("GamepadCheck");

    std::cout << "\n=== Gamepad check ===\n"
              << "A 'controller connected' line should appear above if one is plugged in.\n"
              << "Click the window to focus it, then try:\n"
              << "  Bottom face button    gp_jump (A on Xbox, Cross on PlayStation)\n"
              << "  Right trigger         pull lightly -> aim 1, fire 0; pull fully -> both 1\n"
              << "  Left stick, gently    strength rises while held stays 0  <- the main thing "
                 "to watch\n"
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
