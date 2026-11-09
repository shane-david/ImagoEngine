/**
 * @file InputTests.cpp
 * @author Shane David
 * @brief Tests InputState, InputHandler, and the scheduler's input phases, plus an interactive window check (hidden behind [interactive])
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include "Imago/Engine/Game.hpp"
#include "Imago/Engine/GameConfig.hpp"
#include "Imago/Engine/InputHandler.hpp"
#include "Imago/Engine/InputSource.hpp"
#include "Imago/Engine/InputState.hpp"
#include "Imago/Engine/Scene.hpp"
#include "Imago/Engine/SceneContext.hpp"
#include "Imago/Engine/SystemScheduler.hpp"

using namespace Imago::Engine;
using Scan = sf::Keyboard::Scancode;

// ---------------------------------------------------------------------------
// test access: the friend struct declared in InputState.hpp
// NOTE: define this in ONE test file only, a second definition is an ODR problem
// ---------------------------------------------------------------------------

namespace Imago::Engine
{
    struct InputStateTestAccess {
        static ActionId add_action(InputState& s, const std::string& name) { return s.add_action(name); }
        static void set_strength(InputState& s, ActionId action, float value) { s.set_state(action, value, value > 0.0f); }
        static void clear_edges(InputState& s) { s.clear_edges(); }
        static void begin_frame_phase(InputState& s) { s.begin_frame_phase(); }
        static void end_frame_phase(InputState& s) { s.end_frame_phase(); }
        static void begin_fixed_step(InputState& s) { s.begin_fixed_step(); }
        static void end_fixed_step(InputState& s) { s.end_fixed_step(); }
    };
}

namespace
{
    // --- hand-built SFML events, the event structs are plain aggregates ---
    // NOTE: assumes sf::Event can be built from one of its subtypes, check the Event class reference if this does not compile

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
// InputState (headless, runs every time)
// ---------------------------------------------------------------------------

TEST_CASE("InputState registers actions with sequential ids", "[InputState]")
{
    InputState state;

    REQUIRE(state.action_id("jump") == INVALID_ACTION);

    const ActionId jump = InputStateTestAccess::add_action(state, "jump");
    const ActionId fire = InputStateTestAccess::add_action(state, "fire");

    REQUIRE(jump == 0);
    REQUIRE(fire == 1);
    REQUIRE(state.action_id("jump") == jump);
    REQUIRE(state.action_id("fire") == fire);

    // adding an existing name returns the existing id
    REQUIRE(InputStateTestAccess::add_action(state, "jump") == jump);
}

TEST_CASE("InputState starts every action idle", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    REQUIRE_FALSE(state.held(jump));
    REQUIRE_FALSE(state.began(jump));
    REQUIRE_FALSE(state.ended(jump));
    REQUIRE(state.strength(jump) == Catch::Approx(0.0f));
}

TEST_CASE("InputState set_strength drives held and the edges", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SECTION("rising from 0 sets held, strength, and began")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        REQUIRE(state.held(jump));
        REQUIRE(state.strength(jump) == Catch::Approx(1.0f));
        REQUIRE(state.began(jump));
        REQUIRE_FALSE(state.ended(jump));
    }

    SECTION("falling to 0 sets ended")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 0.0f);

        REQUIRE_FALSE(state.held(jump));
        REQUIRE(state.ended(jump));
        REQUIRE_FALSE(state.began(jump));
    }

    SECTION("changing strength while held makes no new edge")
    {
        InputStateTestAccess::set_strength(state, jump, 0.5f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        REQUIRE(state.held(jump));
        REQUIRE(state.strength(jump) == Catch::Approx(1.0f));
        REQUIRE_FALSE(state.began(jump));
        REQUIRE_FALSE(state.ended(jump));
    }

    SECTION("strength is clamped to 0 through 1")
    {
        InputStateTestAccess::set_strength(state, jump, 2.0f);
        REQUIRE(state.strength(jump) == Catch::Approx(1.0f));

        InputStateTestAccess::set_strength(state, jump, -1.0f);
        REQUIRE(state.strength(jump) == Catch::Approx(0.0f));
        REQUIRE_FALSE(state.held(jump));
    }

    SECTION("a tap keeps both edges while held is already false")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::set_strength(state, jump, 0.0f);

        REQUIRE_FALSE(state.held(jump));
        REQUIRE(state.began(jump));
        REQUIRE(state.ended(jump));
    }

    SECTION("a release then a press before anything reads keeps both edges")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 0.0f);
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        REQUIRE(state.held(jump));
        REQUIRE(state.began(jump));
        REQUIRE(state.ended(jump));
    }
}

TEST_CASE("InputState clear_edges keeps held and strength", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    InputStateTestAccess::set_strength(state, jump, 0.75f);
    InputStateTestAccess::clear_edges(state);

    REQUIRE_FALSE(state.began(jump));
    REQUIRE(state.held(jump));
    REQUIRE(state.strength(jump) == Catch::Approx(0.75f));

    // the fixed edge is cleared too
    InputStateTestAccess::begin_fixed_step(state);
    REQUIRE_FALSE(state.began(jump));
    InputStateTestAccess::end_fixed_step(state);
}

TEST_CASE("InputState edges are phase aware", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SECTION("a fixed edge is consumed by the first step only")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        InputStateTestAccess::begin_fixed_step(state);
        REQUIRE(state.began(jump));
        InputStateTestAccess::end_fixed_step(state);

        InputStateTestAccess::begin_fixed_step(state);
        REQUIRE_FALSE(state.began(jump));
        InputStateTestAccess::end_fixed_step(state);
    }

    SECTION("a fixed edge stays latched through a frame with no fixed step")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        // a frame that runs no fixed step: only the frame phase hooks fire
        InputStateTestAccess::begin_frame_phase(state);
        REQUIRE(state.began(jump));
        InputStateTestAccess::end_frame_phase(state);
        REQUIRE_FALSE(state.began(jump));   // the frame edge is gone

        // the next frame's first step still sees it
        InputStateTestAccess::begin_fixed_step(state);
        REQUIRE(state.began(jump));
        InputStateTestAccess::end_fixed_step(state);
    }

    SECTION("a frame edge survives fixed steps and clears after the frame phase")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        InputStateTestAccess::begin_fixed_step(state);
        InputStateTestAccess::end_fixed_step(state);

        // back in the frame phase, the frame edge was not touched by the fixed step
        REQUIRE(state.began(jump));

        InputStateTestAccess::end_frame_phase(state);
        REQUIRE_FALSE(state.began(jump));
    }

    SECTION("ended edges follow the same rules")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 0.0f);

        InputStateTestAccess::begin_fixed_step(state);
        REQUIRE(state.ended(jump));
        InputStateTestAccess::end_fixed_step(state);

        InputStateTestAccess::begin_fixed_step(state);
        REQUIRE_FALSE(state.ended(jump));
        InputStateTestAccess::end_fixed_step(state);

        REQUIRE(state.ended(jump));   // the frame edge is still waiting
    }
}

TEST_CASE("InputState id overloads match the string overloads", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");
    InputStateTestAccess::set_strength(state, jump, 0.5f);

    REQUIRE(state.held("jump") == state.held(jump));
    REQUIRE(state.began("jump") == state.began(jump));
    REQUIRE(state.ended("jump") == state.ended(jump));
    REQUIRE(state.strength("jump") == Catch::Approx(state.strength(jump)));
}

TEST_CASE("InputState axis combines a negative and a positive action", "[InputState]")
{
    InputState state;
    const ActionId left = InputStateTestAccess::add_action(state, "left");
    const ActionId right = InputStateTestAccess::add_action(state, "right");

    SECTION("nothing pressed is 0")
    {
        REQUIRE(state.axis(left, right) == Catch::Approx(0.0f));
    }

    SECTION("positive and negative")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        REQUIRE(state.axis(left, right) == Catch::Approx(1.0f));

        InputStateTestAccess::set_strength(state, right, 0.0f);
        InputStateTestAccess::set_strength(state, left, 1.0f);
        REQUIRE(state.axis(left, right) == Catch::Approx(-1.0f));
    }

    SECTION("opposite inputs cancel")
    {
        InputStateTestAccess::set_strength(state, left, 1.0f);
        InputStateTestAccess::set_strength(state, right, 1.0f);
        REQUIRE(state.axis(left, right) == Catch::Approx(0.0f));
    }

    SECTION("analog values subtract")
    {
        InputStateTestAccess::set_strength(state, left, 0.25f);
        InputStateTestAccess::set_strength(state, right, 0.5f);
        REQUIRE(state.axis(left, right) == Catch::Approx(0.25f));
    }

    SECTION("the string overload matches")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        REQUIRE(state.axis("left", "right") == Catch::Approx(1.0f));
    }
}

TEST_CASE("InputState direction clamps its length to 1", "[InputState]")
{
    InputState state;
    const ActionId left = InputStateTestAccess::add_action(state, "left");
    const ActionId right = InputStateTestAccess::add_action(state, "right");
    const ActionId up = InputStateTestAccess::add_action(state, "up");
    const ActionId down = InputStateTestAccess::add_action(state, "down");

    SECTION("nothing pressed is a zero vector, not NaN")
    {
        const glm::vec2 dir = state.direction(left, right, up, down);
        REQUIRE(dir.x == Catch::Approx(0.0f));
        REQUIRE(dir.y == Catch::Approx(0.0f));
    }

    SECTION("a single direction is unchanged")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        REQUIRE(dir.x == Catch::Approx(1.0f));
        REQUIRE(dir.y == Catch::Approx(0.0f));
    }

    SECTION("a keyboard diagonal is shortened to length 1")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        InputStateTestAccess::set_strength(state, down, 1.0f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        REQUIRE(dir.x == Catch::Approx(0.7071f).margin(0.001));
        REQUIRE(dir.y == Catch::Approx(0.7071f).margin(0.001));
    }

    SECTION("up is negative y (SFML screen coordinates)")
    {
        InputStateTestAccess::set_strength(state, up, 1.0f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        REQUIRE(dir.y == Catch::Approx(-1.0f));
    }

    SECTION("a light push is left alone")
    {
        InputStateTestAccess::set_strength(state, right, 0.3f);
        InputStateTestAccess::set_strength(state, up, 0.2f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        REQUIRE(dir.x == Catch::Approx(0.3f));
        REQUIRE(dir.y == Catch::Approx(-0.2f));
    }

    SECTION("the string overload matches")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        const glm::vec2 dir = state.direction("left", "right", "up", "down");
        REQUIRE(dir.x == Catch::Approx(1.0f));
    }
}

// the unknown-name checks are asserts in Debug, which would abort the test run, so these only run in Release
#ifdef NDEBUG
TEST_CASE("InputState unknown names and ids return defaults in Release", "[InputState]")
{
    InputState state;

    REQUIRE_FALSE(state.held("nope"));
    REQUIRE_FALSE(state.began("nope"));
    REQUIRE_FALSE(state.ended("nope"));
    REQUIRE(state.strength("nope") == Catch::Approx(0.0f));
    REQUIRE(state.axis("nope", "nada") == Catch::Approx(0.0f));

    const glm::vec2 dir = state.direction("a", "b", "c", "d");
    REQUIRE(dir.x == Catch::Approx(0.0f));
    REQUIRE(dir.y == Catch::Approx(0.0f));

    REQUIRE_FALSE(state.held(INVALID_ACTION));
    REQUIRE_FALSE(state.held(42));
}
#endif

// ---------------------------------------------------------------------------
// InputHandler (headless, hand-built events)
// ---------------------------------------------------------------------------

TEST_CASE("InputHandler register_action returns the same id for a duplicate", "[InputHandler]")
{
    InputHandler handler;

    const ActionId first = handler.register_action("jump");
    const ActionId again = handler.register_action("jump");

    REQUIRE(first == again);
    REQUIRE(handler.state().action_id("jump") == first);
}

TEST_CASE("InputHandler turns key events into action state", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));

    REQUIRE(handler.state().held("jump"));
    REQUIRE(handler.state().began("jump"));
    REQUIRE(handler.state().strength("jump") == Catch::Approx(1.0f));

    handler.handle_event(key_released(Scan::Space));

    REQUIRE_FALSE(handler.state().held("jump"));
    REQUIRE(handler.state().ended("jump"));
    REQUIRE(handler.state().strength("jump") == Catch::Approx(0.0f));
}

TEST_CASE("InputHandler turns mouse button events into action state", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("fire");
    handler.add_input("fire", sf::Mouse::Button::Left);

    handler.handle_event(mouse_pressed(sf::Mouse::Button::Left));
    REQUIRE(handler.state().held("fire"));
    REQUIRE(handler.state().began("fire"));

    handler.handle_event(mouse_released(sf::Mouse::Button::Left));
    REQUIRE_FALSE(handler.state().held("fire"));
    REQUIRE(handler.state().ended("fire"));
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

    REQUIRE(handler.state().held("jump"));
    REQUIRE_FALSE(handler.state().began("jump"));

    // a single release still ends it
    handler.handle_event(key_released(Scan::Space));
    REQUIRE_FALSE(handler.state().held("jump"));
    REQUIRE(handler.state().ended("jump"));
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
    REQUIRE(handler.state().held("move_left"));
    REQUIRE_FALSE(handler.state().ended("move_left"));

    handler.handle_event(key_released(Scan::Left));
    REQUIRE_FALSE(handler.state().held("move_left"));
    REQUIRE(handler.state().ended("move_left"));
}

TEST_CASE("InputHandler lets one input drive several actions", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.register_action("confirm");
    handler.add_input("jump", Scan::Space);
    handler.add_input("confirm", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));

    REQUIRE(handler.state().held("jump"));
    REQUIRE(handler.state().held("confirm"));
}

TEST_CASE("InputHandler ignores inputs that are not added", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Q));
    handler.handle_event(mouse_pressed(sf::Mouse::Button::Right));

    REQUIRE_FALSE(handler.state().held("jump"));
    REQUIRE_FALSE(handler.state().began("jump"));
}

TEST_CASE("InputHandler does not confuse a key and a mouse button", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("from_key");
    handler.register_action("from_mouse");
    handler.add_input("from_key", Scan::A);
    handler.add_input("from_mouse", sf::Mouse::Button::Left);

    handler.handle_event(mouse_pressed(sf::Mouse::Button::Left));

    REQUIRE(handler.state().held("from_mouse"));
    REQUIRE_FALSE(handler.state().held("from_key"));
}

TEST_CASE("InputHandler adding the same input twice behaves like adding it once", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));
    REQUIRE(handler.state().held("jump"));

    handler.handle_event(key_released(Scan::Space));
    REQUIRE_FALSE(handler.state().held("jump"));
}

TEST_CASE("InputHandler focus loss releases everything", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.register_action("fire");
    handler.add_input("jump", Scan::Space);
    handler.add_input("fire", sf::Mouse::Button::Left);

    handler.handle_event(key_pressed(Scan::Space));
    handler.handle_event(mouse_pressed(sf::Mouse::Button::Left));
    handler.clear_edges();

    handler.handle_event(focus_lost());

    REQUIRE_FALSE(handler.state().held("jump"));
    REQUIRE_FALSE(handler.state().held("fire"));
    REQUIRE(handler.state().ended("jump"));
    REQUIRE(handler.state().ended("fire"));

    // a key released while unfocused never sends a release event, so a fresh press must work again
    handler.handle_event(key_pressed(Scan::Space));
    REQUIRE(handler.state().held("jump"));
}

TEST_CASE("InputHandler clear_edges keeps held state", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");
    handler.add_input("jump", Scan::Space);

    handler.handle_event(key_pressed(Scan::Space));
    handler.clear_edges();

    REQUIRE_FALSE(handler.state().began("jump"));
    REQUIRE(handler.state().held("jump"));
}

#ifdef NDEBUG
TEST_CASE("InputHandler ignores bad add_input calls in Release", "[InputHandler]")
{
    InputHandler handler;
    handler.register_action("jump");

    // an Unknown key can never be added, so Unknown events match nothing
    handler.add_input("jump", Scan::Unknown);
    handler.handle_event(key_pressed(Scan::Unknown));
    REQUIRE_FALSE(handler.state().held("jump"));

    // an unregistered action is ignored
    handler.add_input("nope", Scan::Space);
    handler.handle_event(key_pressed(Scan::Space));
    REQUIRE_FALSE(handler.state().held("jump"));
}
#endif

// ---------------------------------------------------------------------------
// SystemScheduler input phases (headless)
// ---------------------------------------------------------------------------

namespace
{
    std::vector<bool> gFixedSaw;   // what began("jump") returned in each fixed step
    std::vector<bool> gFrameSaw;   // what began("jump") returned in each per-frame system call
    int gNoInputCalls = 0;

    void RecordFixed(SceneContext& ctx, float) { gFixedSaw.push_back(ctx.input->began("jump")); }
    void RecordFrame(SceneContext& ctx, float) { gFrameSaw.push_back(ctx.input->began("jump")); }
    void CountCall(SceneContext&, float) { ++gNoInputCalls; }
}

TEST_CASE("SystemScheduler gives a press to the first fixed step only", "[SystemScheduler][Input]")
{
    gFixedSaw.clear();
    gFrameSaw.clear();

    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SceneContext ctx;
    ctx.input = &state;

    SystemScheduler scheduler(ctx);
    scheduler.set_fixed_timestep(1.0f);
    scheduler.register_fixed(RecordFixed);
    scheduler.register_update(RecordFrame);

    InputStateTestAccess::set_strength(state, jump, 1.0f);

    // dt of 3 runs exactly 3 fixed steps in one tick
    scheduler.tick(3.0f);

    REQUIRE(gFixedSaw == std::vector<bool>{true, false, false});
    // the per-frame system runs once and still sees the frame edge
    REQUIRE(gFrameSaw == std::vector<bool>{true});
}

TEST_CASE("SystemScheduler keeps a press latched through a tick with no fixed step", "[SystemScheduler][Input]")
{
    gFixedSaw.clear();
    gFrameSaw.clear();

    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SceneContext ctx;
    ctx.input = &state;

    SystemScheduler scheduler(ctx);
    scheduler.set_fixed_timestep(1.0f);
    scheduler.register_fixed(RecordFixed);
    scheduler.register_update(RecordFrame);

    InputStateTestAccess::set_strength(state, jump, 1.0f);

    // half a step: no fixed step runs, the frame system sees the press, then its edge clears
    scheduler.tick(0.5f);
    REQUIRE(gFixedSaw.empty());
    REQUIRE(gFrameSaw == std::vector<bool>{true});

    // the next half step completes a fixed step, which still sees the press
    scheduler.tick(0.5f);
    REQUIRE(gFixedSaw == std::vector<bool>{true});
    REQUIRE(gFrameSaw == std::vector<bool>{true, false});
}

TEST_CASE("SystemScheduler runs without an InputState", "[SystemScheduler][Input]")
{
    gNoInputCalls = 0;

    SceneContext ctx;   // input stays nullptr
    SystemScheduler scheduler(ctx);
    scheduler.set_fixed_timestep(1.0f);
    scheduler.register_fixed(CountCall);
    scheduler.register_update(CountCall);

    scheduler.tick(2.0f);

    // two fixed steps plus one per-frame call
    REQUIRE(gNoInputCalls == 3);
}

// ---------------------------------------------------------------------------
// Interactive check (needs a real window, hidden: run the test executable with "[interactive]")
// ---------------------------------------------------------------------------

namespace
{
    const char* const DEMO_ACTIONS[] = {"jump", "fire", "alt_fire", "move_left", "move_right", "move_up", "move_down", "quit_demo"};

    // per-frame systems read edges as "this frame"
    void DemoFrameSystem(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        for (const char* name : DEMO_ACTIONS) {
            if (input.began(name)) std::cout << "[frame] began  " << name << std::endl;
            if (input.ended(name)) std::cout << "[frame] ended  " << name << std::endl;
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

    // fixed systems read edges as "since the last fixed step", each press should print here exactly once too
    void DemoFixedSystem(SceneContext& ctx, float)
    {
        InputState& input = *ctx.input;

        for (const char* name : DEMO_ACTIONS) {
            if (input.began(name)) std::cout << "[fixed] began  " << name << std::endl;
            if (input.ended(name)) std::cout << "[fixed] ended  " << name << std::endl;
        }
    }

    class DemoScene : public Scene {
    public:
        using Scene::Scene;
    protected:
        void on_load() override
        {
            _scheduler.register_update(DemoFrameSystem);
            _scheduler.register_fixed(DemoFixedSystem);
        }
    };
}

TEST_CASE("Interactive input check", "[.interactive]")
{
    GameConfig config;
    config.title = "ImagoEngine input check (Esc quits)";
    config.width = 800;
    config.height = 450;
    config.pacing = FramePacing::VSync;
    config.firstScene = "Demo";

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

    game.register_scene<DemoScene>("Demo");

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