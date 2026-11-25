/**
 * @file NexusTests.cpp
 * @brief Tests the Nexus API: entity lifetime, component reads and writes, and component type ids.
 *
 * Structural changes (bind, unbind, destroy) are always deferred until flush(), so every test
 * flushes before reading. Deferral itself and the flush order are covered in CommandBufferTests.cpp.
 */

#include <catch2/catch_test_macros.hpp>

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
}

TEST_CASE("create returns a valid Entity", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    CHECK(nexus.is_valid(e));
}

TEST_CASE("Component type ids are stable per type and unique across types", "[Nexus]")
{
    const ComponentTypeId p1 = get_component_type_id<Position>();
    const ComponentTypeId p2 = get_component_type_id<Position>();
    const ComponentTypeId v = get_component_type_id<Velocity>();

    CHECK(p1 == p2);
    CHECK(p1 != v);
}

TEST_CASE("bind attaches a component and it can be retrieved after flush", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 2});
    nexus.flush();

    REQUIRE(nexus.has<Position>(e));
    CHECK(nexus.get<Position>(e).x == 1);
    CHECK(nexus.get<Position>(e).y == 2);
}

TEST_CASE("bind on an already-bound entity keeps the existing component", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 1});
    nexus.flush();

    nexus.bind<Position>(e, Position{99, 99}); // warns at flush time, does not overwrite
    nexus.flush();

    CHECK(nexus.get<Position>(e).x == 1);
    CHECK(nexus.get<Position>(e).y == 1);
}

TEST_CASE("patch overwrites an existing component immediately", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 1});
    nexus.flush(); // bind still needs a flush to exist in the first place

    nexus.patch<Position>(e, Position{5, 5}); // no flush needed

    CHECK(nexus.get<Position>(e).x == 5);
    CHECK(nexus.get<Position>(e).y == 5);
}

TEST_CASE("get returns a mutable reference into the pool", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{1, 1});
    nexus.flush();

    nexus.get<Position>(e).x = 42;

    CHECK(nexus.get<Position>(e).x == 42);
    CHECK(nexus.try_to_get<Position>(e) == &nexus.get<Position>(e));
}

TEST_CASE("unbind removes a component from an entity after flush", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{1, 1});
    nexus.bind<Velocity>(e, Velocity{2, 2});
    nexus.flush();

    nexus.unbind<Position>(e);
    nexus.flush();

    CHECK_FALSE(nexus.has<Position>(e));
    CHECK(nexus.has<Velocity>(e)); // other components are untouched
    CHECK(nexus.is_valid(e));
}

TEST_CASE("unbind on a component type with no pool yet is a safe no-op", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.unbind<Position>(e); // Position pool never created
    REQUIRE_NOTHROW(nexus.flush());

    CHECK_FALSE(nexus.has<Position>(e));
}

TEST_CASE("has returns false for a component type never bound to anything", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    CHECK_FALSE(nexus.has<Position>(e)); // no pool for Position exists at all yet
}

TEST_CASE("try_to_get returns nullptr when no pool exists for the component type", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    CHECK(nexus.try_to_get<Position>(e) == nullptr);
}

TEST_CASE("try_to_get returns nullptr when a pool exists but this entity has no component",
          "[Nexus]")
{
    Nexus nexus;
    Entity a = nexus.create();
    Entity b = nexus.create();

    nexus.bind<Position>(a, Position{1, 1}); // creates the Position pool
    nexus.flush();

    CHECK(nexus.try_to_get<Position>(b) == nullptr);
}

TEST_CASE("try_to_get returns a valid pointer when the component exists", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();
    nexus.bind<Position>(e, Position{3, 4});
    nexus.flush();

    Position* p = nexus.try_to_get<Position>(e);

    REQUIRE(p != nullptr);
    CHECK(p->x == 3);
    CHECK(p->y == 4);
}

TEST_CASE("An entity can hold multiple different component types at once", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 1});
    nexus.bind<Velocity>(e, Velocity{2, 2});
    nexus.flush();

    CHECK(nexus.has<Position>(e));
    REQUIRE(nexus.has<Velocity>(e));
    CHECK(nexus.get<Velocity>(e).dx == 2);
}

TEST_CASE("destroy removes the entity from every pool it was in", "[Nexus]")
{
    Nexus nexus;
    Entity e = nexus.create();

    nexus.bind<Position>(e, Position{1, 1});
    nexus.bind<Velocity>(e, Velocity{2, 2});
    nexus.flush();

    nexus.destroy(e);
    nexus.flush();

    CHECK_FALSE(nexus.is_valid(e));
    CHECK_FALSE(nexus.has<Position>(e));
    CHECK_FALSE(nexus.has<Velocity>(e));
}

TEST_CASE("Destroying one entity does not affect another entity's components", "[Nexus]")
{
    Nexus nexus;
    Entity a = nexus.create();
    Entity b = nexus.create();

    nexus.bind<Position>(a, Position{1, 1});
    nexus.bind<Position>(b, Position{9, 9});
    nexus.flush();

    nexus.destroy(a);
    nexus.flush();

    CHECK_FALSE(nexus.has<Position>(a));
    REQUIRE(nexus.has<Position>(b));
    CHECK(nexus.get<Position>(b).x == 9);
}

TEST_CASE("A destroyed entity's index is recycled with a new generation", "[Nexus]")
{
    Nexus nexus;
    Entity old = nexus.create();
    nexus.destroy(old);
    nexus.flush();

    Entity recycled = nexus.create();

    CHECK(get_entity_index(recycled) == get_entity_index(old));
    CHECK(get_entity_generation(recycled) == get_entity_generation(old) + 1);
    CHECK(nexus.is_valid(recycled));
    CHECK_FALSE(nexus.is_valid(old));
}

// NOTE: [!mayfail] - the pools only key on the Entity's index, not its generation. After old's
// index is reused, has<T>(old) and try_to_get<T>(old) currently answer for the NEW entity
// (true / non-null) even though is_valid(old) is false.
TEST_CASE("A stale handle does not see the components of the entity that reused its index",
          "[Nexus][!mayfail]")
{
    Nexus nexus;
    Entity old = nexus.create();
    nexus.destroy(old);
    nexus.flush();

    Entity recycled = nexus.create();
    nexus.bind<Position>(recycled, Position{7, 7});
    nexus.flush();
    REQUIRE(get_entity_index(recycled) == get_entity_index(old));
    REQUIRE_FALSE(nexus.is_valid(old));

    CHECK_FALSE(nexus.has<Position>(old));
    CHECK(nexus.try_to_get<Position>(old) == nullptr);
}
