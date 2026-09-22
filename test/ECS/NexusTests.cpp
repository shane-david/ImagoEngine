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

TEST_CASE("Create returns a valid Entity", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  REQUIRE(nexus.IsValid(e));
}

TEST_CASE("Bind attaches a component and it can be retrieved", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  nexus.Bind<Position>(e, Position{ 1, 2 });

  REQUIRE(nexus.Has<Position>(e));
  REQUIRE(nexus.Get<Position>(e).x == 1);
  REQUIRE(nexus.Get<Position>(e).y == 2);
}

TEST_CASE("Bind on an already-bound entity returns the existing component", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  nexus.Bind<Position>(e, Position{ 1, 1 });
  Position& existing = nexus.Bind<Position>(e, Position{ 99, 99 }); // should warn, not overwrite

  REQUIRE(existing.x == 1);
  REQUIRE(nexus.Get<Position>(e).x == 1);
}

TEST_CASE("Patch overwrites an existing component", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  nexus.Bind<Position>(e, Position{ 1, 1 });
  nexus.Patch<Position>(e, Position{ 5, 5 });

  REQUIRE(nexus.Get<Position>(e).x == 5);
  REQUIRE(nexus.Get<Position>(e).y == 5);
}

TEST_CASE("Unbind removes a component from an entity", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();
  nexus.Bind<Position>(e, Position{ 1, 1 });

  nexus.Unbind<Position>(e);

  REQUIRE_FALSE(nexus.Has<Position>(e));
}

TEST_CASE("Unbind on a component type with no pool yet is a safe no-op", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  REQUIRE_NOTHROW(nexus.Unbind<Position>(e)); // Position pool never created
}

TEST_CASE("Has returns false for a component type never bound to anything", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  REQUIRE_FALSE(nexus.Has<Position>(e)); // no pool for Position exists at all yet
}

TEST_CASE("TryGet returns nullptr when no pool exists for the component type", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  REQUIRE(nexus.TryToGet<Position>(e) == nullptr);
}

TEST_CASE("TryGet returns nullptr when a pool exists but this entity has no component", "[nexus]")
{
  Nexus nexus;
  Entity a = nexus.Create();
  Entity b = nexus.Create();

  nexus.Bind<Position>(a, Position{ 1, 1 }); // creates the Position pool

  REQUIRE(nexus.TryToGet<Position>(b) == nullptr);
}

TEST_CASE("TryGet returns a valid pointer when the component exists", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();
  nexus.Bind<Position>(e, Position{ 3, 4 });

  Position* p = nexus.TryToGet<Position>(e);

  REQUIRE(p != nullptr);
  REQUIRE(p->x == 3);
}

TEST_CASE("An entity can hold multiple different component types at once", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  nexus.Bind<Position>(e, Position{ 1, 1 });
  nexus.Bind<Velocity>(e, Velocity{ 2, 2 });

  REQUIRE(nexus.Has<Position>(e));
  REQUIRE(nexus.Has<Velocity>(e));
  REQUIRE(nexus.Get<Velocity>(e).dx == 2);
}

TEST_CASE("Destroy removes the entity from every pool it was in", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.Create();

  nexus.Bind<Position>(e, Position{ 1, 1 });
  nexus.Bind<Velocity>(e, Velocity{ 2, 2 });

  nexus.Destroy(e);

  REQUIRE_FALSE(nexus.IsValid(e));
  REQUIRE_FALSE(nexus.Has<Position>(e));
  REQUIRE_FALSE(nexus.Has<Velocity>(e));
}

TEST_CASE("Destroying one entity does not affect another entity's components", "[nexus]")
{
  Nexus nexus;
  Entity a = nexus.Create();
  Entity b = nexus.Create();

  nexus.Bind<Position>(a, Position{ 1, 1 });
  nexus.Bind<Position>(b, Position{ 9, 9 });

  nexus.Destroy(a);

  REQUIRE_FALSE(nexus.Has<Position>(a));
  REQUIRE(nexus.Has<Position>(b));
  REQUIRE(nexus.Get<Position>(b).x == 9);
}