/**
 * @file SystemSchedulerTests.cpp
 * @brief Tests SystemScheduler: per-frame core and user systems, fixed-step accumulation, the
 * max-step cap, get_alpha, registration, and ordering.
 *
 * Input phase behavior during tick() is tested in InputStateTests.cpp.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "Imago/Engine/SceneContext.hpp"
#include "Imago/Engine/SystemScheduler.hpp"

using namespace Imago::Engine;

namespace
{
    using Calls = std::vector<std::string>;

    // every test system writes into these, so tests can check what ran, in what order, and with
    // what dt
    Calls gCalls;                           ///< system names, in the order they were called
    std::vector<float> gDts;                ///< the dt each call received, in the same order
    const SceneContext* gSeenCtx = nullptr; ///< the SceneContext the last see_ctx call received

    void clear_log()
    {
        gCalls.clear();
        gDts.clear();
        gSeenCtx = nullptr;
    }

    void record(const char* name, float dt)
    {
        gCalls.push_back(name);
        gDts.push_back(dt);
    }

    int count_calls(const std::string& name)
    {
        return static_cast<int>(std::count(gCalls.begin(), gCalls.end(), name));
    }

    // test systems: free functions, since SystemFn is a function pointer
    void fixed_a(SceneContext&, float dt)
    {
        record("fixed_a", dt);
    }
    void fixed_b(SceneContext&, float dt)
    {
        record("fixed_b", dt);
    }
    void core_a(SceneContext&, float dt)
    {
        record("core_a", dt);
    }
    void core_b(SceneContext&, float dt)
    {
        record("core_b", dt);
    }
    void user_a(SceneContext&, float dt)
    {
        record("user_a", dt);
    }
    void user_b(SceneContext&, float dt)
    {
        record("user_b", dt);
    }
    void user_c(SceneContext&, float dt)
    {
        record("user_c", dt);
    }

    void see_ctx(SceneContext& ctx, float)
    {
        gSeenCtx = &ctx;
    }
}

// NOTE: tests use fixed steps and dt values like 0.5f, 0.25f, 0.0625f and 1.5f on purpose.
// They are exact in binary floating point, so step counts never depend on rounding.

TEST_CASE("Update systems run once per tick with the real dt", "[SystemScheduler]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.register_update(user_a);
    scheduler.tick(0.25f);

    CHECK(gCalls == Calls{"user_a"});
    CHECK(gDts == std::vector<float>{0.25f});

    scheduler.tick(0.75f);

    CHECK(gCalls == Calls{"user_a", "user_a"});
    CHECK(gDts == std::vector<float>{0.25f, 0.75f});
}

TEST_CASE("Core systems run before user systems, whatever the registration order",
          "[SystemScheduler]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.register_update(user_a);
    scheduler.register_core(core_a);

    scheduler.tick(0.1f);

    CHECK(gCalls == Calls{"core_a", "user_a"});
}

TEST_CASE("Systems run in registration order within a list", "[SystemScheduler]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    SECTION("core systems")
    {
        scheduler.register_core(core_b);
        scheduler.register_core(core_a);

        scheduler.tick(0.1f);

        CHECK(gCalls == Calls{"core_b", "core_a"});
    }

    SECTION("user systems")
    {
        scheduler.register_update(user_c);
        scheduler.register_update(user_a);
        scheduler.register_update(user_b);

        scheduler.tick(0.1f);

        CHECK(gCalls == Calls{"user_c", "user_a", "user_b"});
    }
}

TEST_CASE("Fixed systems run once per whole step of accumulated time",
          "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);
    scheduler.register_fixed(fixed_a);

    SECTION("less than one step does not run them")
    {
        scheduler.tick(0.25f);
        CHECK(count_calls("fixed_a") == 0);
    }

    SECTION("exactly one step runs them once")
    {
        scheduler.tick(0.5f);
        CHECK(count_calls("fixed_a") == 1);
    }

    SECTION("several steps run them several times")
    {
        scheduler.tick(1.5f);
        CHECK(count_calls("fixed_a") == 3);
    }

    SECTION("leftover time carries into the next tick")
    {
        scheduler.tick(0.25f);
        CHECK(count_calls("fixed_a") == 0);

        scheduler.tick(0.25f);
        CHECK(count_calls("fixed_a") == 1);
    }

    SECTION("the remainder after some steps is kept too")
    {
        scheduler.tick(0.75f); // one step, 0.25 left over
        CHECK(count_calls("fixed_a") == 1);

        scheduler.tick(0.25f); // 0.25 + 0.25 is one more step
        CHECK(count_calls("fixed_a") == 2);
    }
}

TEST_CASE("Many small ticks accumulate into whole fixed steps", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);
    scheduler.register_fixed(fixed_a);

    // 7 ticks of 1/16 s are 0.4375 s, still under one step
    for (int i = 0; i < 7; ++i) {
        scheduler.tick(0.0625f);
    }
    CHECK(count_calls("fixed_a") == 0);
    CHECK(scheduler.get_alpha() == Catch::Approx(0.875f));

    // the 8th tick reaches exactly 0.5 s
    scheduler.tick(0.0625f);
    CHECK(count_calls("fixed_a") == 1);
    CHECK(scheduler.get_alpha() == Catch::Approx(0.0f));

    // another 16 ticks are two more steps
    for (int i = 0; i < 16; ++i) {
        scheduler.tick(0.0625f);
    }
    CHECK(count_calls("fixed_a") == 3);
}

TEST_CASE("Fixed systems receive the fixed step and update systems receive the frame time",
          "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);
    scheduler.register_fixed(fixed_a);
    scheduler.register_update(user_a);

    scheduler.tick(1.5f);

    CHECK(gCalls == Calls{"fixed_a", "fixed_a", "fixed_a", "user_a"});
    CHECK(gDts == std::vector<float>{0.5f, 0.5f, 0.5f, 1.5f});
}

TEST_CASE("The fixed phase runs before the update phases", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);

    // registered in the opposite order to how they should run
    scheduler.register_update(user_a);
    scheduler.register_core(core_a);
    scheduler.register_fixed(fixed_a);

    scheduler.tick(1.0f);

    CHECK(gCalls == Calls{"fixed_a", "fixed_a", "core_a", "user_a"});
}

TEST_CASE("Fixed systems run in registration order on every step", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);
    scheduler.register_fixed(fixed_a);
    scheduler.register_fixed(fixed_b);

    scheduler.tick(1.0f);

    CHECK(gCalls == Calls{"fixed_a", "fixed_b", "fixed_a", "fixed_b"});
}

TEST_CASE("The step cap limits fixed steps per tick", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);
    scheduler.set_max_fixed_steps(4);
    scheduler.register_fixed(fixed_a);

    SECTION("a tick under the cap is unaffected")
    {
        scheduler.tick(1.0f);
        CHECK(count_calls("fixed_a") == 2);
    }

    SECTION("a long tick is capped and the backlog is dropped")
    {
        scheduler.tick(10.0f); // 20 steps owed, only 4 run
        CHECK(count_calls("fixed_a") == 4);

        clear_log();
        scheduler.tick(0.0f); // if the backlog had been kept, 4 more would run here
        CHECK(count_calls("fixed_a") == 0);
    }

    SECTION("the fractional remainder survives the backlog drop")
    {
        scheduler.tick(10.25f); // 4 steps run, the owed whole steps are dropped, 0.25 is kept
        CHECK(count_calls("fixed_a") == 4);

        clear_log();
        scheduler.tick(0.25f); // 0.25 kept + 0.25 is exactly one step
        CHECK(count_calls("fixed_a") == 1);
    }

    SECTION("reaching the cap exactly keeps the remainder")
    {
        scheduler.tick(2.25f); // 4 steps run and 0.25 is left, so no backlog to drop
        CHECK(count_calls("fixed_a") == 4);

        clear_log();
        scheduler.tick(0.25f);
        CHECK(count_calls("fixed_a") == 1);
    }

    SECTION("the cap holds on every one of several long ticks")
    {
        scheduler.tick(10.0f);
        scheduler.tick(10.0f);
        scheduler.tick(10.0f);
        CHECK(count_calls("fixed_a") == 12);
    }
}

TEST_CASE("A cap of one step runs at most one step per tick", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);
    scheduler.set_max_fixed_steps(1);
    scheduler.register_fixed(fixed_a);

    scheduler.tick(10.25f); // 1 step runs, the backlog is dropped, 0.25 is kept

    CHECK(count_calls("fixed_a") == 1);
    CHECK(scheduler.get_alpha() == Catch::Approx(0.5f));
}

TEST_CASE("get_alpha stays below 1 after a capped tick", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);
    scheduler.set_max_fixed_steps(2);

    scheduler.tick(7.0f); // a whole number of steps owed: nothing is left after the drop
    CHECK(scheduler.get_alpha() >= 0.0f);
    CHECK(scheduler.get_alpha() < 1.0f);
    CHECK(scheduler.get_alpha() == Catch::Approx(0.0f));

    scheduler.tick(5.375f); // 0.375 left after the drop
    CHECK(scheduler.get_alpha() < 1.0f);
    CHECK(scheduler.get_alpha() == Catch::Approx(0.75f));
}

TEST_CASE("The default fixed step is 1/60 s and the default cap is 8 steps",
          "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.register_fixed(fixed_a);

    SECTION("default step")
    {
        // same expression as the class default, so the two floats are identical
        scheduler.tick(1.0f / 60.0f);
        CHECK(count_calls("fixed_a") == 1);
    }

    SECTION("default cap")
    {
        scheduler.tick(10.0f); // 600 steps owed
        CHECK(count_calls("fixed_a") == 8);
    }
}

TEST_CASE("set_fixed_timestep changes the step size", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.25f);
    scheduler.register_fixed(fixed_a);

    scheduler.tick(1.0f);

    CHECK(gDts == std::vector<float>{0.25f, 0.25f, 0.25f, 0.25f});
}

TEST_CASE("get_alpha reports the fraction of a step left over", "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.set_fixed_timestep(0.5f);

    CHECK(scheduler.get_alpha() == Catch::Approx(0.0f));

    scheduler.tick(0.125f);
    CHECK(scheduler.get_alpha() == Catch::Approx(0.25f));

    scheduler.tick(0.375f); // accumulator reaches exactly one step, which is consumed
    CHECK(scheduler.get_alpha() == Catch::Approx(0.0f));

    scheduler.tick(0.25f);
    CHECK(scheduler.get_alpha() == Catch::Approx(0.5f));
}

TEST_CASE("Registering the same system twice only runs it once", "[SystemScheduler]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    SECTION("core")
    {
        scheduler.register_core(core_a);
        scheduler.register_core(core_a);

        scheduler.tick(0.1f);

        CHECK(gCalls == Calls{"core_a"});
    }

    SECTION("update")
    {
        scheduler.register_update(user_a);
        scheduler.register_update(user_a);

        scheduler.tick(0.1f);

        CHECK(gCalls == Calls{"user_a"});
    }

    SECTION("fixed")
    {
        scheduler.set_fixed_timestep(0.5f);
        scheduler.register_fixed(fixed_a);
        scheduler.register_fixed(fixed_a);

        scheduler.tick(0.5f);

        CHECK(gCalls == Calls{"fixed_a"});
    }
}

TEST_CASE("Unregister removes a system", "[SystemScheduler]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    SECTION("from the core list")
    {
        scheduler.register_core(core_a);
        scheduler.register_core(core_b);

        scheduler.unregister(core_a);
        scheduler.tick(0.1f);

        CHECK(gCalls == Calls{"core_b"});
    }

    SECTION("from the user list")
    {
        scheduler.register_update(user_a);
        scheduler.register_update(user_b);

        scheduler.unregister(user_a);
        scheduler.tick(0.1f);

        CHECK(gCalls == Calls{"user_b"});
    }

    SECTION("from the fixed list")
    {
        scheduler.set_fixed_timestep(0.5f);
        scheduler.register_fixed(fixed_a);
        scheduler.register_fixed(fixed_b);

        scheduler.unregister(fixed_a);
        scheduler.tick(0.5f);

        CHECK(gCalls == Calls{"fixed_b"});
    }

    SECTION("from every list it is in")
    {
        scheduler.set_fixed_timestep(0.5f);
        scheduler.register_core(core_a);
        scheduler.register_update(core_a);
        scheduler.register_fixed(core_a);

        scheduler.unregister(core_a);
        scheduler.tick(1.0f);

        CHECK(gCalls.empty());
    }

    SECTION("a system that was never registered is harmless")
    {
        scheduler.register_update(user_b);

        scheduler.unregister(user_a); // logs a warning and does nothing
        scheduler.tick(0.1f);

        CHECK(gCalls == Calls{"user_b"});
    }
}

TEST_CASE("set_user_order reorders only the user systems", "[SystemScheduler]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.register_core(core_a);
    scheduler.register_update(user_a);
    scheduler.register_update(user_b);
    scheduler.register_update(user_c);

    scheduler.set_user_order(std::vector<SystemScheduler::SystemFn>{user_c, user_a, user_b});
    scheduler.tick(0.1f);

    CHECK(gCalls == Calls{"core_a", "user_c", "user_a", "user_b"});
}

TEST_CASE("Systems receive the SceneContext the scheduler was built with", "[SystemScheduler]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    SECTION("update systems")
    {
        scheduler.register_update(see_ctx);
        scheduler.tick(0.1f);
        CHECK(gSeenCtx == &ctx);
    }

    SECTION("fixed systems")
    {
        scheduler.set_fixed_timestep(0.5f);
        scheduler.register_fixed(see_ctx);
        scheduler.tick(0.5f);
        CHECK(gSeenCtx == &ctx);
    }
}

TEST_CASE("A tick with a dt of zero runs update systems but no fixed steps",
          "[SystemScheduler][FixedStep]")
{
    clear_log();
    SceneContext ctx;
    SystemScheduler scheduler(ctx);

    scheduler.register_fixed(fixed_a);
    scheduler.register_update(user_a);

    scheduler.tick(0.0f);

    CHECK(gCalls == Calls{"user_a"});
}
