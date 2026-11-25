/**
 * @file CommandBufferTests.cpp
 * @brief Tests deferred structural changes: bind, unbind, and destroy are queued by the Nexus's
 * CommandBuffer and only applied on flush(), in the order binds, then unbinds, then destroys.
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "Imago/ECS/CommandBuffer.hpp"
#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/Nexus.hpp"

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

    using Entities = std::vector<Entity>;

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

// --- nothing structural happens before flush ---

TEST_CASE("create is immediate and never deferred", "[CommandBuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();

    CHECK(nexus.is_valid(e)); // no flush needed
}

TEST_CASE("bind does not take effect until flush", "[CommandBuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 2});

    CHECK_FALSE(nexus.has<Position>(e)); // not applied yet
}

TEST_CASE("try_to_get returns nullptr before a queued bind is flushed", "[CommandBuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{3, 4});

    CHECK(nexus.try_to_get<Position>(e) == nullptr); // queued, not applied yet
}

TEST_CASE("unbind does not take effect until flush", "[CommandBuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{1, 1});
    nexus.flush();

    nexus.unbind<Position>(e);

    CHECK(nexus.has<Position>(e)); // still bound - flush() hasn't run yet
}

TEST_CASE("destroy does not take effect until flush", "[CommandBuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{1, 1});
    nexus.flush();

    nexus.destroy(e);

    CHECK(nexus.is_valid(e)); // still alive - flush() hasn't run yet
    CHECK(nexus.has<Position>(e));
}

// --- flush order: binds, then unbinds, then destroys ---

TEST_CASE("A bind and a destroy for the same entity, queued before any flush, both resolve in "
          "one flush",
          "[CommandBuffer][FlushOrder]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 1});
    nexus.destroy(e);
    nexus.flush();

    // binds are applied before destroys within a single flush, so the component
    // briefly exists before the destroy's cross-pool cleanup removes it again
    CHECK_FALSE(nexus.is_valid(e));
    CHECK_FALSE(nexus.has<Position>(e));
}

TEST_CASE("A destroy queued before a bind still runs last", "[CommandBuffer][FlushOrder]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.destroy(e);
    nexus.bind<Position>(e, Position{1, 1});
    nexus.flush();

    CHECK_FALSE(nexus.is_valid(e));
    CHECK_FALSE(nexus.has<Position>(e));
    CHECK(collect(nexus.survey<Position>()).empty()); // nothing left behind in the pool
}

TEST_CASE("A bind then an unbind in one flush leaves the entity without the component",
          "[CommandBuffer][FlushOrder]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 1});
    nexus.unbind<Position>(e);
    nexus.flush();

    CHECK_FALSE(nexus.has<Position>(e));
    CHECK(nexus.is_valid(e));
}

TEST_CASE("An unbind queued before a bind still leaves the entity without the component",
          "[CommandBuffer][FlushOrder]")
{
    Nexus nexus;
    Entity e = nexus.create();

    SECTION("when the entity already had it")
    {
        nexus.bind<Position>(e, Position{1, 1});
        nexus.flush();

        // the bind is a no-op on an entity that already has Position, then the unbind removes it
        nexus.unbind<Position>(e);
        nexus.bind<Position>(e, Position{2, 2});
        nexus.flush();

        CHECK_FALSE(nexus.has<Position>(e));
    }

    SECTION("when the entity did not have it")
    {
        // the bind runs first and adds it, then the unbind removes it again
        nexus.unbind<Position>(e);
        nexus.bind<Position>(e, Position{2, 2});
        nexus.flush();

        CHECK_FALSE(nexus.has<Position>(e));
    }
}

TEST_CASE("An unbind and a destroy for the same entity resolve in one flush",
          "[CommandBuffer][FlushOrder]")
{
    Nexus nexus;
    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{1, 1});
    nexus.bind<Velocity>(e, Velocity{1, 1});
    nexus.flush();

    nexus.destroy(e);
    nexus.unbind<Position>(e);
    nexus.flush();

    CHECK_FALSE(nexus.is_valid(e));
    CHECK_FALSE(nexus.has<Position>(e));
    CHECK_FALSE(nexus.has<Velocity>(e));
}

TEST_CASE("Binds apply in the order they were queued", "[CommandBuffer][FlushOrder]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 1});
    nexus.bind<Position>(e, Position{2, 2}); // a second bind of the same type warns and is ignored
    nexus.flush();

    REQUIRE(nexus.has<Position>(e));
    CHECK(nexus.get<Position>(e).x == 1);
}

TEST_CASE("One flush applies every queued command across many entities", "[CommandBuffer]")
{
    Nexus nexus;
    Entities entities;
    for (int i = 0; i < 5; ++i) {
        Entity e = nexus.create();
        entities.push_back(e);
        nexus.bind<Position>(e, Position{i, i});
    }
    nexus.flush();

    nexus.bind<Velocity>(entities[0], Velocity{1, 0});
    nexus.bind<Velocity>(entities[1], Velocity{2, 0});
    nexus.unbind<Position>(entities[2]);
    nexus.destroy(entities[3]);
    nexus.flush();

    CHECK(collect(nexus.survey<Position, Velocity>()) == sorted({entities[0], entities[1]}));
    CHECK(collect(nexus.survey<Position>()) == sorted({entities[0], entities[1], entities[4]}));
    CHECK(nexus.is_valid(entities[2]));
    CHECK_FALSE(nexus.is_valid(entities[3]));
}

TEST_CASE("flush empties the queues so nothing is replayed", "[CommandBuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();

    SECTION("a bind is not replayed after an unbind")
    {
        nexus.bind<Position>(e, Position{1, 1});
        nexus.flush();
        nexus.unbind<Position>(e);
        nexus.flush();

        nexus.flush(); // an empty flush

        CHECK_FALSE(nexus.has<Position>(e));
    }

    SECTION("a destroy is not replayed onto the entity that reused the index")
    {
        nexus.destroy(e);
        nexus.flush();

        Entity recycled = nexus.create();
        REQUIRE(get_entity_index(recycled) == get_entity_index(e));
        nexus.bind<Position>(recycled, Position{3, 3});
        nexus.flush();

        CHECK(nexus.is_valid(recycled));
        CHECK(nexus.has<Position>(recycled));
    }

    SECTION("an empty flush is harmless")
    {
        REQUIRE_NOTHROW(nexus.flush());
        CHECK(nexus.is_valid(e));
    }
}

TEST_CASE("Destroying the same entity twice in one flush is harmless", "[CommandBuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();
    Entity other = nexus.create();
    nexus.bind<Position>(e, Position{1, 1});
    nexus.bind<Position>(other, Position{2, 2});
    nexus.flush();

    nexus.destroy(e);
    nexus.destroy(e); // the second one warns and does nothing
    nexus.flush();

    CHECK_FALSE(nexus.is_valid(e));
    REQUIRE(nexus.has<Position>(other));
    CHECK(nexus.get<Position>(other).x == 2);

    // the index was only freed once
    Entity first = nexus.create();
    Entity second = nexus.create();
    CHECK(get_entity_index(first) == get_entity_index(e));
    CHECK(get_entity_index(second) != get_entity_index(e));
}

TEST_CASE("Deferred destroys queued while iterating a bond do not disturb the loop",
          "[CommandBuffer][Bond]")
{
    Nexus nexus;
    Entities entities;
    for (int i = 0; i < 4; ++i) {
        Entity e = nexus.create();
        entities.push_back(e);
        nexus.bind<Position>(e, Position{i, 0});
        nexus.bind<Velocity>(e, Velocity{i, 0});
    }
    nexus.flush();

    Entities visited;
    for (Entity e : nexus.bond<Position, Velocity>()) {
        visited.push_back(e);
        nexus.destroy(e);
        nexus.unbind<Velocity>(e);
    }

    CHECK(sorted(visited) == sorted(entities));
    CHECK(collect(nexus.bond<Position, Velocity>()) == sorted(entities)); // not applied yet

    nexus.flush();
    CHECK(collect(nexus.bond<Position, Velocity>()).empty());
}

TEST_CASE("Bond membership is right when a bind, an unbind, and a destroy land in one flush",
          "[CommandBuffer][Bond]")
{
    Nexus nexus;
    Entity joins = nexus.create();
    Entity leaves = nexus.create();
    Entity destroyed = nexus.create();
    Entity stays = nexus.create();

    nexus.bind<Position>(joins, Position{1, 0});
    for (Entity e : {leaves, destroyed, stays}) {
        nexus.bind<Position>(e, Position{static_cast<int>(e), 0});
        nexus.bind<Velocity>(e, Velocity{static_cast<int>(e), 0});
    }
    nexus.flush();
    REQUIRE(collect(nexus.bond<Position, Velocity>()) == sorted({leaves, destroyed, stays}));

    nexus.bind<Velocity>(joins, Velocity{1, 0});
    nexus.unbind<Position>(leaves);
    nexus.destroy(destroyed);
    nexus.flush();

    CHECK(collect(nexus.bond<Position, Velocity>()) == sorted({joins, stays}));
    CHECK(nexus.get<Position>(stays).x == static_cast<int>(stays));
    CHECK(nexus.get<Velocity>(joins).dx == 1);
    CHECK(nexus.has<Velocity>(leaves));
}

// --- a CommandBuffer used directly ---

TEST_CASE("A standalone CommandBuffer defers into its Nexus and flushes on its own",
          "[CommandBuffer]")
{
    Nexus nexus;
    CommandBuffer buffer(nexus);
    Entity keep = nexus.create();
    Entity doomed = nexus.create();

    buffer.defer_bind<Position>(keep, Position{4, 2});
    buffer.defer_bind<Velocity>(doomed, Velocity{1, 1});
    buffer.defer_destroy(doomed);

    CHECK_FALSE(nexus.has<Position>(keep));

    // the Nexus's own flush only drains the Nexus's buffer, not this one
    nexus.flush();
    CHECK_FALSE(nexus.has<Position>(keep));
    CHECK(nexus.is_valid(doomed));

    buffer.flush();
    REQUIRE(nexus.has<Position>(keep));
    CHECK(nexus.get<Position>(keep).x == 4);
    CHECK_FALSE(nexus.is_valid(doomed));
    CHECK_FALSE(nexus.has<Velocity>(doomed));
}

TEST_CASE("A standalone CommandBuffer applies binds, then unbinds, then destroys",
          "[CommandBuffer][FlushOrder]")
{
    Nexus nexus;
    CommandBuffer buffer(nexus);
    Entity e = nexus.create();

    buffer.defer_destroy(e);
    buffer.defer_unbind<Position>(e);
    buffer.defer_bind<Position>(e, Position{1, 1});
    buffer.defer_bind<Velocity>(e, Velocity{1, 1});
    buffer.flush();

    CHECK_FALSE(nexus.is_valid(e));
    CHECK_FALSE(nexus.has<Position>(e));
    CHECK_FALSE(nexus.has<Velocity>(e));
}
