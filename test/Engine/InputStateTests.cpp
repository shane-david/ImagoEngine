/**
 * @file InputStateTests.cpp
 * @brief Tests InputState, the read-only action view systems query: ids, held/strength, edges,
 * axis/direction, and how began/ended behave differently in fixed and per-frame phases.
 *
 * The phase tests at the bottom drive InputState through the public InputHandler and
 * SystemScheduler, the same path Game uses.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <glm/vec2.hpp>

#include "Imago/Engine/InputHandler.hpp"
#include "Imago/Engine/InputSource.hpp"
#include "Imago/Engine/InputState.hpp"
#include "Imago/Engine/SceneContext.hpp"
#include "Imago/Engine/SystemScheduler.hpp"
#include "Support/EngineTestAccess.hpp"

using namespace Imago::Engine;

// ---------------------------------------------------------------------------
// InputState written directly through InputStateTestAccess
// ---------------------------------------------------------------------------

TEST_CASE("InputState registers actions with sequential ids", "[InputState]")
{
    InputState state;

    REQUIRE(state.action_id("jump") == INVALID_ACTION);

    const ActionId jump = InputStateTestAccess::add_action(state, "jump");
    const ActionId fire = InputStateTestAccess::add_action(state, "fire");

    CHECK(jump == 0);
    CHECK(fire == 1);
    CHECK(state.action_id("jump") == jump);
    CHECK(state.action_id("fire") == fire);

    // adding an existing name returns the existing id
    CHECK(InputStateTestAccess::add_action(state, "jump") == jump);
}

TEST_CASE("InputState starts every action idle", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    CHECK_FALSE(state.held(jump));
    CHECK_FALSE(state.began(jump));
    CHECK_FALSE(state.ended(jump));
    CHECK(state.strength(jump) == Catch::Approx(0.0f));
}

TEST_CASE("InputState set_strength drives held and the edges", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SECTION("rising from 0 sets held, strength, and began")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        CHECK(state.held(jump));
        CHECK(state.strength(jump) == Catch::Approx(1.0f));
        CHECK(state.began(jump));
        CHECK_FALSE(state.ended(jump));
    }

    SECTION("falling to 0 sets ended")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 0.0f);

        CHECK_FALSE(state.held(jump));
        CHECK(state.ended(jump));
        CHECK_FALSE(state.began(jump));
    }

    SECTION("changing strength while held makes no new edge")
    {
        InputStateTestAccess::set_strength(state, jump, 0.5f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        CHECK(state.held(jump));
        CHECK(state.strength(jump) == Catch::Approx(1.0f));
        CHECK_FALSE(state.began(jump));
        CHECK_FALSE(state.ended(jump));
    }

    SECTION("strength is clamped to 0 through 1")
    {
        InputStateTestAccess::set_strength(state, jump, 2.0f);
        CHECK(state.strength(jump) == Catch::Approx(1.0f));

        InputStateTestAccess::set_strength(state, jump, -1.0f);
        CHECK(state.strength(jump) == Catch::Approx(0.0f));
        CHECK_FALSE(state.held(jump));
    }

    SECTION("a tap keeps both edges while held is already false")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::set_strength(state, jump, 0.0f);

        CHECK_FALSE(state.held(jump));
        CHECK(state.began(jump));
        CHECK(state.ended(jump));
    }

    SECTION("a release then a press before anything reads keeps both edges")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 0.0f);
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        CHECK(state.held(jump));
        CHECK(state.began(jump));
        CHECK(state.ended(jump));
    }
}

TEST_CASE("InputState clear_edges keeps held and strength", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    InputStateTestAccess::set_strength(state, jump, 0.75f);
    InputStateTestAccess::clear_edges(state);

    CHECK_FALSE(state.began(jump));
    CHECK(state.held(jump));
    CHECK(state.strength(jump) == Catch::Approx(0.75f));

    // the fixed edge is cleared too
    InputStateTestAccess::begin_fixed_step(state);
    CHECK_FALSE(state.began(jump));
    InputStateTestAccess::end_fixed_step(state);
}

TEST_CASE("InputState edges are phase aware", "[InputState][Phase]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SECTION("a fixed edge is consumed by the first step only")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        InputStateTestAccess::begin_fixed_step(state);
        CHECK(state.began(jump));
        InputStateTestAccess::end_fixed_step(state);

        InputStateTestAccess::begin_fixed_step(state);
        CHECK_FALSE(state.began(jump));
        InputStateTestAccess::end_fixed_step(state);
    }

    SECTION("a fixed edge stays latched through a frame with no fixed step")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        // a frame that runs no fixed step: only the frame phase hooks fire
        InputStateTestAccess::begin_frame_phase(state);
        CHECK(state.began(jump));
        InputStateTestAccess::end_frame_phase(state);
        CHECK_FALSE(state.began(jump)); // the frame edge is gone

        // the next frame's first step still sees it
        InputStateTestAccess::begin_fixed_step(state);
        CHECK(state.began(jump));
        InputStateTestAccess::end_fixed_step(state);
    }

    SECTION("a frame edge survives fixed steps and clears after the frame phase")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);

        InputStateTestAccess::begin_fixed_step(state);
        InputStateTestAccess::end_fixed_step(state);

        // back in the frame phase, the frame edge was not touched by the fixed step
        CHECK(state.began(jump));

        InputStateTestAccess::end_frame_phase(state);
        CHECK_FALSE(state.began(jump));
    }

    SECTION("ended edges follow the same rules")
    {
        InputStateTestAccess::set_strength(state, jump, 1.0f);
        InputStateTestAccess::clear_edges(state);
        InputStateTestAccess::set_strength(state, jump, 0.0f);

        InputStateTestAccess::begin_fixed_step(state);
        CHECK(state.ended(jump));
        InputStateTestAccess::end_fixed_step(state);

        InputStateTestAccess::begin_fixed_step(state);
        CHECK_FALSE(state.ended(jump));
        InputStateTestAccess::end_fixed_step(state);

        CHECK(state.ended(jump)); // the frame edge is still waiting
    }
}

TEST_CASE("InputState id overloads match the string overloads", "[InputState]")
{
    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");
    InputStateTestAccess::set_strength(state, jump, 0.5f);

    CHECK(state.held("jump") == state.held(jump));
    CHECK(state.began("jump") == state.began(jump));
    CHECK(state.ended("jump") == state.ended(jump));
    CHECK(state.strength("jump") == Catch::Approx(state.strength(jump)));
}

TEST_CASE("InputState axis combines a negative and a positive action", "[InputState]")
{
    InputState state;
    const ActionId left = InputStateTestAccess::add_action(state, "left");
    const ActionId right = InputStateTestAccess::add_action(state, "right");

    SECTION("nothing pressed is 0")
    {
        CHECK(state.axis(left, right) == Catch::Approx(0.0f));
    }

    SECTION("positive and negative")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        CHECK(state.axis(left, right) == Catch::Approx(1.0f));

        InputStateTestAccess::set_strength(state, right, 0.0f);
        InputStateTestAccess::set_strength(state, left, 1.0f);
        CHECK(state.axis(left, right) == Catch::Approx(-1.0f));
    }

    SECTION("opposite inputs cancel")
    {
        InputStateTestAccess::set_strength(state, left, 1.0f);
        InputStateTestAccess::set_strength(state, right, 1.0f);
        CHECK(state.axis(left, right) == Catch::Approx(0.0f));
    }

    SECTION("analog values subtract")
    {
        InputStateTestAccess::set_strength(state, left, 0.25f);
        InputStateTestAccess::set_strength(state, right, 0.5f);
        CHECK(state.axis(left, right) == Catch::Approx(0.25f));
    }

    SECTION("the string overload matches")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        CHECK(state.axis("left", "right") == Catch::Approx(1.0f));
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
        CHECK(dir.x == Catch::Approx(0.0f));
        CHECK(dir.y == Catch::Approx(0.0f));
    }

    SECTION("a single direction is unchanged")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        CHECK(dir.x == Catch::Approx(1.0f));
        CHECK(dir.y == Catch::Approx(0.0f));
    }

    SECTION("a keyboard diagonal is shortened to length 1")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        InputStateTestAccess::set_strength(state, down, 1.0f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        CHECK(dir.x == Catch::Approx(0.7071f).margin(0.001));
        CHECK(dir.y == Catch::Approx(0.7071f).margin(0.001));
    }

    SECTION("up is negative y (SFML screen coordinates)")
    {
        InputStateTestAccess::set_strength(state, up, 1.0f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        CHECK(dir.y == Catch::Approx(-1.0f));
    }

    SECTION("a light push is left alone")
    {
        InputStateTestAccess::set_strength(state, right, 0.3f);
        InputStateTestAccess::set_strength(state, up, 0.2f);
        const glm::vec2 dir = state.direction(left, right, up, down);
        CHECK(dir.x == Catch::Approx(0.3f));
        CHECK(dir.y == Catch::Approx(-0.2f));
    }

    SECTION("the string overload matches")
    {
        InputStateTestAccess::set_strength(state, right, 1.0f);
        const glm::vec2 dir = state.direction("left", "right", "up", "down");
        CHECK(dir.x == Catch::Approx(1.0f));
    }
}

// the unknown-name checks are asserts in Debug, which would abort the test run, so these only
// run in Release
#ifdef NDEBUG
TEST_CASE("InputState unknown names and ids return defaults in Release", "[InputState]")
{
    InputState state;

    CHECK_FALSE(state.held("nope"));
    CHECK_FALSE(state.began("nope"));
    CHECK_FALSE(state.ended("nope"));
    CHECK(state.strength("nope") == Catch::Approx(0.0f));
    CHECK(state.axis("nope", "nada") == Catch::Approx(0.0f));

    const glm::vec2 dir = state.direction("a", "b", "c", "d");
    CHECK(dir.x == Catch::Approx(0.0f));
    CHECK(dir.y == Catch::Approx(0.0f));

    CHECK_FALSE(state.held(INVALID_ACTION));
    CHECK_FALSE(state.held(42));
}
#endif

// ---------------------------------------------------------------------------
// Phases driven by the SystemScheduler (the path Game uses)
// ---------------------------------------------------------------------------

namespace
{
    /// @brief What one system call saw for the "jump" action.
    struct Seen {
        bool began;
        bool ended;
        bool held;

        bool operator==(const Seen& o) const
        {
            return began == o.began && ended == o.ended && held == o.held;
        }
    };

    std::vector<Seen> gFixedSaw; ///< one entry per fixed step
    std::vector<Seen> gFrameSaw; ///< one entry per per-frame system call
    int gNoInputCalls = 0;

    Seen look(const SceneContext& ctx)
    {
        return Seen{ctx.input->began("jump"), ctx.input->ended("jump"), ctx.input->held("jump")};
    }

    void record_fixed(SceneContext& ctx, float)
    {
        gFixedSaw.push_back(look(ctx));
    }
    void record_frame(SceneContext& ctx, float)
    {
        gFrameSaw.push_back(look(ctx));
    }
    void count_call(SceneContext&, float)
    {
        ++gNoInputCalls;
    }

    constexpr Seen NOTHING{false, false, false};
    constexpr Seen HELD{false, false, true};
    constexpr Seen PRESSED{true, false, true};
    constexpr Seen RELEASED{false, true, false};
    constexpr Seen TAPPED{true, true, false};

    /// @brief An InputHandler with "jump" on FaceDown, wired into a scheduler with step 1.0 that
    /// records what its fixed and frame systems see.
    struct PhaseRig {
        InputHandler handler;
        SceneContext ctx;
        SystemScheduler scheduler{ctx};

        PhaseRig()
        {
            gFixedSaw.clear();
            gFrameSaw.clear();
            handler.register_action("jump");
            handler.add_input("jump", GamepadButton::FaceDown);
            ctx.input = &handler.state();
            scheduler.set_fixed_timestep(1.0f);
            scheduler.register_fixed(record_fixed);
            scheduler.register_update(record_frame);
        }

        void press() { handler.handle_gamepad_button(GamepadButton::FaceDown, true); }
        void release() { handler.handle_gamepad_button(GamepadButton::FaceDown, false); }
    };
}

TEST_CASE("SystemScheduler gives a press to the first fixed step only", "[InputState][Phase]")
{
    gFixedSaw.clear();
    gFrameSaw.clear();

    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SceneContext ctx;
    ctx.input = &state;

    SystemScheduler scheduler(ctx);
    scheduler.set_fixed_timestep(1.0f);
    scheduler.register_fixed(record_fixed);
    scheduler.register_update(record_frame);

    InputStateTestAccess::set_strength(state, jump, 1.0f);

    // dt of 3 runs exactly 3 fixed steps in one tick
    scheduler.tick(3.0f);

    CHECK(gFixedSaw == std::vector<Seen>{PRESSED, HELD, HELD});
    // the per-frame system runs once and still sees the frame edge
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED});
}

TEST_CASE("SystemScheduler keeps a press latched through a tick with no fixed step",
          "[InputState][Phase]")
{
    gFixedSaw.clear();
    gFrameSaw.clear();

    InputState state;
    const ActionId jump = InputStateTestAccess::add_action(state, "jump");

    SceneContext ctx;
    ctx.input = &state;

    SystemScheduler scheduler(ctx);
    scheduler.set_fixed_timestep(1.0f);
    scheduler.register_fixed(record_fixed);
    scheduler.register_update(record_frame);

    InputStateTestAccess::set_strength(state, jump, 1.0f);

    // half a step: no fixed step runs, the frame system sees the press, then its edge clears
    scheduler.tick(0.5f);
    CHECK(gFixedSaw.empty());
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED});

    // the next half step completes a fixed step, which still sees the press
    scheduler.tick(0.5f);
    CHECK(gFixedSaw == std::vector<Seen>{PRESSED});
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED, HELD});
}

TEST_CASE("SystemScheduler runs without an InputState", "[InputState][Phase]")
{
    gNoInputCalls = 0;

    SceneContext ctx; // input stays nullptr
    SystemScheduler scheduler(ctx);
    scheduler.set_fixed_timestep(1.0f);
    scheduler.register_fixed(count_call);
    scheduler.register_update(count_call);

    scheduler.tick(2.0f);

    // two fixed steps plus one per-frame call
    CHECK(gNoInputCalls == 3);
}

TEST_CASE("A gamepad press reaches only the first of several fixed steps and the frame once",
          "[InputState][Phase]")
{
    PhaseRig rig;

    rig.press();
    rig.scheduler.tick(3.0f);

    CHECK(gFixedSaw == std::vector<Seen>{PRESSED, HELD, HELD});
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED});

    // a following tick sees no edges at all, only held
    rig.scheduler.tick(1.0f);
    CHECK(gFixedSaw == std::vector<Seen>{PRESSED, HELD, HELD, HELD});
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED, HELD});
}

TEST_CASE("A press in a tick with no fixed step is latched for the next tick's first step",
          "[InputState][Phase]")
{
    PhaseRig rig;

    rig.press();
    rig.scheduler.tick(0.5f); // no fixed step
    REQUIRE(gFixedSaw.empty());
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED});

    rig.scheduler.tick(0.5f); // one fixed step
    CHECK(gFixedSaw == std::vector<Seen>{PRESSED});
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED, HELD}); // the frame edge was already used
}

TEST_CASE("A tap between ticks shows both edges in the first fixed step and the frame",
          "[InputState][Phase]")
{
    PhaseRig rig;

    rig.press();
    rig.release();
    rig.scheduler.tick(2.0f);

    CHECK(gFixedSaw == std::vector<Seen>{TAPPED, NOTHING});
    CHECK(gFrameSaw == std::vector<Seen>{TAPPED});
}

TEST_CASE("A release follows the same phase rules as a press", "[InputState][Phase]")
{
    PhaseRig rig;
    rig.press();
    rig.scheduler.tick(1.0f);
    gFixedSaw.clear();
    gFrameSaw.clear();

    SECTION("with several fixed steps")
    {
        rig.release();
        rig.scheduler.tick(2.0f);

        CHECK(gFixedSaw == std::vector<Seen>{RELEASED, NOTHING});
        CHECK(gFrameSaw == std::vector<Seen>{RELEASED});
    }

    SECTION("with no fixed step, the fixed edge waits for the next tick")
    {
        rig.release();
        rig.scheduler.tick(0.5f);
        REQUIRE(gFixedSaw.empty());
        CHECK(gFrameSaw == std::vector<Seen>{RELEASED});

        rig.scheduler.tick(0.5f);
        CHECK(gFixedSaw == std::vector<Seen>{RELEASED});
        CHECK(gFrameSaw == std::vector<Seen>{RELEASED, NOTHING});
    }
}

TEST_CASE("held reads the same in fixed and frame phases", "[InputState][Phase]")
{
    PhaseRig rig;

    rig.press();
    rig.scheduler.tick(2.0f);
    rig.scheduler.tick(0.5f);

    for (const Seen& seen : gFixedSaw) {
        CHECK(seen.held);
    }
    for (const Seen& seen : gFrameSaw) {
        CHECK(seen.held);
    }

    // outside any system the state still reports held, and no edge is left over
    CHECK(rig.handler.state().held("jump"));
    CHECK_FALSE(rig.handler.state().began("jump"));
}

TEST_CASE("clear_edges drops a latched fixed edge, as on a scene switch", "[InputState][Phase]")
{
    PhaseRig rig;

    rig.press();
    rig.scheduler.tick(0.5f); // the fixed edge is latched, no step has consumed it
    rig.handler.clear_edges();

    rig.scheduler.tick(0.5f);

    CHECK(gFixedSaw == std::vector<Seen>{HELD});
    CHECK(gFrameSaw == std::vector<Seen>{PRESSED, HELD});
}
