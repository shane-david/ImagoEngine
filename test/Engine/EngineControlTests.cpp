/**
 * @file EngineControlTests.cpp
 * @brief Tests EngineControl, the request recorder systems use to switch scenes and quit.
 *
 * The Game-level rule that a quit beats a pending switch is tested in GameTests.cpp, since it
 * needs a real window.
 */

#include <catch2/catch_test_macros.hpp>

#include "Imago/Engine/EngineControl.hpp"
#include "Support/EngineTestAccess.hpp"

using namespace Imago::Engine;

TEST_CASE("EngineControl starts with nothing requested", "[EngineControl]")
{
    EngineControl engine;

    CHECK_FALSE(EngineControlTestAccess::has_pending(engine));
    CHECK_FALSE(EngineControlTestAccess::quit_requested(engine));
    CHECK_FALSE(EngineControlTestAccess::take(engine).has_value());
}

TEST_CASE("EngineControl switch_scene records the name and defaults persist to false",
          "[EngineControl]")
{
    EngineControl engine;

    engine.switch_scene("Level1");

    const auto request = EngineControlTestAccess::take(engine);
    REQUIRE(request.has_value());
    CHECK(request->name == "Level1");
    CHECK(request->persist == false);
}

TEST_CASE("EngineControl switch_scene records persist when asked", "[EngineControl]")
{
    EngineControl engine;

    engine.switch_scene("Pause", true);

    const auto request = EngineControlTestAccess::take(engine);
    REQUIRE(request.has_value());
    CHECK(request->name == "Pause");
    CHECK(request->persist == true);
}

TEST_CASE("EngineControl last switch_scene call in a frame wins", "[EngineControl]")
{
    EngineControl engine;

    SECTION("a later request replaces both the name and persist")
    {
        engine.switch_scene("First", true);
        engine.switch_scene("Second", false);

        const auto request = EngineControlTestAccess::take(engine);
        REQUIRE(request.has_value());
        CHECK(request->name == "Second");
        CHECK(request->persist == false);
    }

    SECTION("a later persist request is not lost to an earlier non-persist one")
    {
        engine.switch_scene("First", false);
        engine.switch_scene("Second", false);
        engine.switch_scene("Third", true);

        const auto request = EngineControlTestAccess::take(engine);
        REQUIRE(request.has_value());
        CHECK(request->name == "Third");
        CHECK(request->persist == true);
    }

    SECTION("the same scene requested twice with different persist keeps the last")
    {
        engine.switch_scene("Level", true);
        engine.switch_scene("Level", false);

        const auto request = EngineControlTestAccess::take(engine);
        REQUIRE(request.has_value());
        CHECK(request->name == "Level");
        CHECK(request->persist == false);
    }
}

TEST_CASE("EngineControl take_switch_request consumes the request once", "[EngineControl]")
{
    EngineControl engine;
    engine.switch_scene("Level1");

    CHECK(EngineControlTestAccess::take(engine).has_value());
    CHECK_FALSE(EngineControlTestAccess::has_pending(engine));
    CHECK_FALSE(EngineControlTestAccess::take(engine).has_value());
}

TEST_CASE("EngineControl records a new request after the previous one was taken", "[EngineControl]")
{
    EngineControl engine;
    engine.switch_scene("Level1");
    REQUIRE(EngineControlTestAccess::take(engine).has_value());

    engine.switch_scene("Level2", true);

    const auto request = EngineControlTestAccess::take(engine);
    REQUIRE(request.has_value());
    CHECK(request->name == "Level2");
    CHECK(request->persist == true);
}

TEST_CASE("EngineControl quit sets the flag and is idempotent", "[EngineControl]")
{
    EngineControl engine;

    engine.quit();
    CHECK(EngineControlTestAccess::quit_requested(engine));

    engine.quit();
    CHECK(EngineControlTestAccess::quit_requested(engine));
}

TEST_CASE("EngineControl quit and take do not affect each other", "[EngineControl]")
{
    EngineControl engine;

    SECTION("quit leaves a pending switch alone")
    {
        engine.switch_scene("Level1");
        engine.quit();

        CHECK(EngineControlTestAccess::has_pending(engine));
        CHECK(EngineControlTestAccess::quit_requested(engine));
    }

    SECTION("taking a switch does not clear quit")
    {
        engine.quit();
        engine.switch_scene("Level1");
        EngineControlTestAccess::take(engine);

        CHECK(EngineControlTestAccess::quit_requested(engine));
    }
}

// the empty-name check is an assert in Debug, which would abort the test run, so it only runs in
// Release
#ifdef NDEBUG
TEST_CASE("EngineControl ignores an empty scene name in Release", "[EngineControl]")
{
    EngineControl engine;

    engine.switch_scene("");

    CHECK_FALSE(EngineControlTestAccess::has_pending(engine));
}
#endif
