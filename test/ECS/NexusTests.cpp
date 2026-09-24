// test/ECS/NexusTests.cpp
#include <catch2/catch_test_macros.hpp>

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/Nexus.hpp"

using namespace Imago::ECS;

namespace
{
  struct Position
  {
    int x = 0;
    int y = 0;
  };

  struct Velocity
  {
    int dx = 0;
    int dy = 0;
  };
}

TEST_CASE("create returns a valid Entity", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  REQUIRE(nexus.is_valid(e));
}

TEST_CASE("bind attaches a component and it can be retrieved", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 2 });

  REQUIRE(nexus.has<Position>(e));
  REQUIRE(nexus.get<Position>(e).x == 1);
  REQUIRE(nexus.get<Position>(e).y == 2);
}

TEST_CASE("bind on an already-bound entity returns the existing component", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  Position& existing = nexus.bind<Position>(e, Position{ 99, 99 }); // should warn, not overwrite

  REQUIRE(existing.x == 1);
  REQUIRE(nexus.get<Position>(e).x == 1);
}

TEST_CASE("patch overwrites an existing component", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.patch<Position>(e, Position{ 5, 5 });

  REQUIRE(nexus.get<Position>(e).x == 5);
  REQUIRE(nexus.get<Position>(e).y == 5);
}

TEST_CASE("unbind removes a component from an entity", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();
  nexus.bind<Position>(e, Position{ 1, 1 });

  nexus.unbind<Position>(e);

  REQUIRE_FALSE(nexus.has<Position>(e));
}

TEST_CASE("unbind on a component type with no pool yet is a safe no-op", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  REQUIRE_NOTHROW(nexus.unbind<Position>(e)); // Position pool never created
}

TEST_CASE("has returns false for a component type never bound to anything", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  REQUIRE_FALSE(nexus.has<Position>(e)); // no pool for Position exists at all yet
}

TEST_CASE("try_to_get returns nullptr when no pool exists for the component type", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  REQUIRE(nexus.try_to_get<Position>(e) == nullptr);
}

TEST_CASE("try_to_get returns nullptr when a pool exists but this entity has no component", "[nexus]")
{
  Nexus nexus;
  Entity a = nexus.create();
  Entity b = nexus.create();

  nexus.bind<Position>(a, Position{ 1, 1 }); // creates the Position pool

  REQUIRE(nexus.try_to_get<Position>(b) == nullptr);
}

TEST_CASE("try_to_get returns a valid pointer when the component exists", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();
  nexus.bind<Position>(e, Position{ 3, 4 });

  Position* p = nexus.try_to_get<Position>(e);

  REQUIRE(p != nullptr);
  REQUIRE(p->x == 3);
}

TEST_CASE("An entity can hold multiple different component types at once", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.bind<Velocity>(e, Velocity{ 2, 2 });

  REQUIRE(nexus.has<Position>(e));
  REQUIRE(nexus.has<Velocity>(e));
  REQUIRE(nexus.get<Velocity>(e).dx == 2);
}

TEST_CASE("destroy removes the entity from every pool it was in", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.bind<Velocity>(e, Velocity{ 2, 2 });

  nexus.destroy(e);

  REQUIRE_FALSE(nexus.is_valid(e));
  REQUIRE_FALSE(nexus.has<Position>(e));
  REQUIRE_FALSE(nexus.has<Velocity>(e));
}

TEST_CASE("Destroying one entity does not affect another entity's components", "[nexus]")
{
  Nexus nexus;
  Entity a = nexus.create();
  Entity b = nexus.create();

  nexus.bind<Position>(a, Position{ 1, 1 });
  nexus.bind<Position>(b, Position{ 9, 9 });

  nexus.destroy(a);

  REQUIRE_FALSE(nexus.has<Position>(a));
  REQUIRE(nexus.has<Position>(b));
  REQUIRE(nexus.get<Position>(b).x == 9);
}