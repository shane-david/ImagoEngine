/**
 * @file SurveyHandleTests.cpp
 * @brief Tests SurveyHandle, both built directly over SparseSets and obtained through
 * Nexus::survey<Components...>().
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/Nexus.hpp"
#include "Imago/ECS/SparseSet.hpp"
#include "Imago/ECS/SparseSetBase.hpp"
#include "Imago/ECS/SurveyHandle.hpp"

using namespace Imago::ECS;

namespace
{
    struct Position {
        int x = 0;
        int y = 0;
    };

    struct Velocity {
        int dx = 0;
        int dy = 0;
    };

    struct Health {
        int hp = 0;
    };

    using Entities = std::vector<Entity>;

    /// @brief Gathers every Entity a survey yields, sorted so tests never depend on pool order.
    template <typename Handle> Entities collect(const Handle& handle)
    {
        Entities result;
        for (Entity e : handle) {
            result.push_back(e);
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    Entities sorted(Entities entities)
    {
        std::sort(entities.begin(), entities.end());
        return entities;
    }
}

// --- SurveyHandle constructed directly, outside of Nexus ---

TEST_CASE("SurveyHandle yields an entity present in every requested pool", "[SurveyHandle]")
{
    SparseSet<Position> positions;
    SparseSet<Velocity> velocities;

    Entity e = make_entity(0, 0);
    positions.insert(e, Position{1, 1});
    velocities.insert(e, Velocity{2, 2});

    std::array<SparseSetBase*, 2> pools = {&positions, &velocities};
    SurveyHandle<Position, Velocity> survey(pools);

    CHECK(collect(survey) == Entities{e});
}

TEST_CASE("SurveyHandle excludes an entity missing one of the requested components",
          "[SurveyHandle]")
{
    SparseSet<Position> positions;
    SparseSet<Velocity> velocities;

    Entity onlyPosition = make_entity(0, 0);
    Entity both = make_entity(1, 0);

    positions.insert(onlyPosition, Position{1, 1});

    positions.insert(both, Position{2, 2});
    velocities.insert(both, Velocity{3, 3});

    std::array<SparseSetBase*, 2> pools = {&positions, &velocities};
    SurveyHandle<Position, Velocity> survey(pools);

    CHECK(collect(survey) == Entities{both});
}

TEST_CASE("SurveyHandle yields nothing when the smallest pool is empty", "[SurveyHandle]")
{
    SparseSet<Position> positions;
    SparseSet<Velocity> velocities;

    Entity e = make_entity(0, 0);
    positions.insert(e, Position{1, 1}); // velocities pool stays empty

    std::array<SparseSetBase*, 2> pools = {&positions, &velocities};
    SurveyHandle<Position, Velocity> survey(pools);

    CHECK(collect(survey).empty());
}

TEST_CASE("SurveyHandle correctly selects whichever pool is smaller, regardless of argument order",
          "[SurveyHandle]")
{
    SparseSet<Position> positions;
    SparseSet<Velocity> velocities;

    // velocities is deliberately the larger pool, positions the smaller
    Entity match = make_entity(0, 0);
    positions.insert(match, Position{1, 1});
    velocities.insert(match, Velocity{1, 1});

    for (uint32_t i = 1; i <= 5; ++i) {
        velocities.insert(make_entity(i, 0), Velocity{});
    }

    SECTION("smaller pool first")
    {
        std::array<SparseSetBase*, 2> pools = {&positions, &velocities};
        SurveyHandle<Position, Velocity> survey(pools);
        CHECK(collect(survey) == Entities{match});
    }

    SECTION("smaller pool last")
    {
        std::array<SparseSetBase*, 2> pools = {&velocities, &positions};
        SurveyHandle<Velocity, Position> survey(pools);
        CHECK(collect(survey) == Entities{match});
    }
}

// --- Survey obtained through Nexus::survey<Components...>() ---

TEST_CASE("nexus.survey yields entities with every requested component", "[SurveyHandle][Nexus]")
{
    Nexus nexus;

    Entity a = nexus.create();
    nexus.bind<Position>(a, Position{1, 1});
    nexus.bind<Velocity>(a, Velocity{1, 1});

    Entity b = nexus.create();
    nexus.bind<Position>(b, Position{2, 2}); // no Velocity - should be excluded
    nexus.flush();

    CHECK(collect(nexus.survey<Position, Velocity>()) == Entities{a});
}

TEST_CASE("nexus.survey reflects component data correctly through get", "[SurveyHandle][Nexus]")
{
    Nexus nexus;

    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{5, 6});
    nexus.bind<Velocity>(e, Velocity{1, 1});
    nexus.flush();

    int visited = 0;
    for (Entity found : nexus.survey<Position, Velocity>()) {
        Position& pos = nexus.get<Position>(found);
        CHECK(pos.x == 5);
        CHECK(pos.y == 6);
        ++visited;
    }
    CHECK(visited == 1);
}

TEST_CASE("nexus.survey yields multiple matching entities", "[SurveyHandle][Nexus]")
{
    Nexus nexus;

    Entity a = nexus.create();
    Entity b = nexus.create();
    Entity c = nexus.create();

    nexus.bind<Position>(a, Position{});
    nexus.bind<Velocity>(a, Velocity{});

    nexus.bind<Position>(b, Position{});
    nexus.bind<Velocity>(b, Velocity{});

    nexus.bind<Position>(c, Position{}); // no Velocity - excluded
    nexus.flush();

    CHECK(collect(nexus.survey<Position, Velocity>()) == sorted({a, b}));
}

TEST_CASE("nexus.survey for a component type with no pool yet yields nothing",
          "[SurveyHandle][Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{}); // Velocity pool never created
    nexus.flush();

    // survey() creates the missing pool empty, so the survey is empty rather than invalid
    CHECK(collect(nexus.survey<Position, Velocity>()).empty());
}

TEST_CASE("nexus.survey of a single component yields every holder", "[SurveyHandle][Nexus]")
{
    Nexus nexus;
    Entity a = nexus.create();
    Entity b = nexus.create();
    nexus.create(); // no components at all
    nexus.bind<Health>(a, Health{1});
    nexus.bind<Health>(b, Health{2});
    nexus.flush();

    CHECK(collect(nexus.survey<Health>()) == sorted({a, b}));
}

TEST_CASE("nexus.survey of three components needs all three", "[SurveyHandle][Nexus]")
{
    Nexus nexus;
    Entity all = nexus.create();
    Entity two = nexus.create();
    Entity one = nexus.create();

    nexus.bind<Position>(all, Position{});
    nexus.bind<Velocity>(all, Velocity{});
    nexus.bind<Health>(all, Health{});

    nexus.bind<Position>(two, Position{});
    nexus.bind<Health>(two, Health{});

    nexus.bind<Health>(one, Health{});
    nexus.flush();

    CHECK(collect(nexus.survey<Position, Velocity, Health>()) == Entities{all});
    CHECK(collect(nexus.survey<Health, Position>()) == sorted({all, two}));
}

TEST_CASE("nexus.survey reflects unbinds and destroys once flushed", "[SurveyHandle][Nexus]")
{
    Nexus nexus;
    Entity a = nexus.create();
    Entity b = nexus.create();
    Entity c = nexus.create();
    for (Entity e : {a, b, c}) {
        nexus.bind<Position>(e, Position{});
        nexus.bind<Velocity>(e, Velocity{});
    }
    nexus.flush();
    REQUIRE(collect(nexus.survey<Position, Velocity>()) == sorted({a, b, c}));

    nexus.unbind<Velocity>(a);
    nexus.destroy(b);

    // nothing has changed until the flush
    CHECK(collect(nexus.survey<Position, Velocity>()) == sorted({a, b, c}));

    nexus.flush();
    CHECK(collect(nexus.survey<Position, Velocity>()) == Entities{c});
}

TEST_CASE("Deferred changes queued while iterating a survey do not disturb the loop",
          "[SurveyHandle][Nexus]")
{
    Nexus nexus;
    Entities created;
    for (int i = 0; i < 6; ++i) {
        Entity e = nexus.create();
        created.push_back(e);
        nexus.bind<Position>(e, Position{i, 0});
    }
    nexus.flush();

    // destroy every other entity and tag the rest while the survey is running
    Entities visited;
    for (Entity e : nexus.survey<Position>()) {
        visited.push_back(e);
        if (nexus.get<Position>(e).x % 2 == 0) {
            nexus.destroy(e);
        } else {
            nexus.bind<Velocity>(e, Velocity{1, 1});
        }
    }

    CHECK(sorted(visited) == sorted(created));

    nexus.flush();
    CHECK(collect(nexus.survey<Position>()) == sorted({created[1], created[3], created[5]}));
    CHECK(collect(nexus.survey<Position, Velocity>()) ==
          sorted({created[1], created[3], created[5]}));
}
