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

// --- create is immediate - not structural, never deferred ---

TEST_CASE("create returns a valid Entity immediately", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  REQUIRE(nexus.is_valid(e));
}

// --- bind is deferred until flush ---

TEST_CASE("bind does not take effect until flush", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 2 });

  REQUIRE_FALSE(nexus.has<Position>(e)); // not applied yet
}

TEST_CASE("flush applies a queued bind", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 2 });
  nexus.flush();

  REQUIRE(nexus.has<Position>(e));
  REQUIRE(nexus.get<Position>(e).x == 1);
  REQUIRE(nexus.get<Position>(e).y == 2);
}

TEST_CASE("Binding an already-bound entity does not overwrite it, after flush", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.flush();

  nexus.bind<Position>(e, Position{ 99, 99 }); // should warn at flush time, not overwrite
  nexus.flush();

  REQUIRE(nexus.get<Position>(e).x == 1);
}

// --- patch stays immediate - overwrites in place, no deferral needed ---

TEST_CASE("patch overwrites an existing component immediately, no flush needed", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.flush(); // bind still needs a flush to exist in the first place

  nexus.patch<Position>(e, Position{ 5, 5 });

  REQUIRE(nexus.get<Position>(e).x == 5); // took effect immediately - no second flush needed
}

// --- unbind is deferred until flush ---

TEST_CASE("unbind does not take effect until flush", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();
  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.flush();

  nexus.unbind<Position>(e);

  REQUIRE(nexus.has<Position>(e)); // still bound - flush() hasn't run yet
}

TEST_CASE("flush applies a queued unbind", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();
  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.flush();

  nexus.unbind<Position>(e);
  nexus.flush();

  REQUIRE_FALSE(nexus.has<Position>(e));
}

TEST_CASE("unbind on a component type with no pool yet is still safe after flush", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.unbind<Position>(e); // Position pool never created
  REQUIRE_NOTHROW(nexus.flush());
}

// --- has / get / try_to_get - reads, always reflect only what's been flushed so far ---

TEST_CASE("has returns false for a component type never bound to anything", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  REQUIRE_FALSE(nexus.has<Position>(e));
}

TEST_CASE("try_to_get returns nullptr before a queued bind is flushed", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 3, 4 });

  REQUIRE(nexus.try_to_get<Position>(e) == nullptr); // queued, not applied yet
}

TEST_CASE("try_to_get returns a valid pointer once the queued bind is flushed", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 3, 4 });
  nexus.flush();

  Position* p = nexus.try_to_get<Position>(e);

  REQUIRE(p != nullptr);
  REQUIRE(p->x == 3);
}

TEST_CASE("An entity can hold multiple different component types at once, after flush", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.bind<Velocity>(e, Velocity{ 2, 2 });
  nexus.flush();

  REQUIRE(nexus.has<Position>(e));
  REQUIRE(nexus.has<Velocity>(e));
  REQUIRE(nexus.get<Velocity>(e).dx == 2);
}

// --- destroy is deferred until flush ---

TEST_CASE("destroy does not take effect until flush", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.destroy(e);

  REQUIRE(nexus.is_valid(e)); // still alive - flush() hasn't run yet
}

TEST_CASE("flush applies a queued destroy, removing the entity from every pool it was in", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();
  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.bind<Velocity>(e, Velocity{ 2, 2 });
  nexus.flush();

  nexus.destroy(e);
  nexus.flush();

  REQUIRE_FALSE(nexus.is_valid(e));
  REQUIRE_FALSE(nexus.has<Position>(e));
  REQUIRE_FALSE(nexus.has<Velocity>(e));
}

TEST_CASE("Destroying one entity does not affect another entity's components, after flush", "[nexus]")
{
  Nexus nexus;
  Entity a = nexus.create();
  Entity b = nexus.create();

  nexus.bind<Position>(a, Position{ 1, 1 });
  nexus.bind<Position>(b, Position{ 9, 9 });
  nexus.flush();

  nexus.destroy(a);
  nexus.flush();

  REQUIRE_FALSE(nexus.has<Position>(a));
  REQUIRE(nexus.has<Position>(b));
  REQUIRE(nexus.get<Position>(b).x == 9);
}

// --- a single flush can resolve a full create -> bind -> destroy sequence at once ---

TEST_CASE("A bind and a destroy for the same entity, queued before any flush, both resolve in one flush", "[nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();

  nexus.bind<Position>(e, Position{ 1, 1 });
  nexus.destroy(e);
  nexus.flush();

  // binds are applied before destroys within a single flush, so the component
  // briefly exists before the destroy's cross-pool cleanup removes it again
  REQUIRE_FALSE(nexus.is_valid(e));
  REQUIRE_FALSE(nexus.has<Position>(e));
}