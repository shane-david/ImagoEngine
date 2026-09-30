// test/ECS/SurveyTests.cpp
#include <array>

#include <catch2/catch_test_macros.hpp>

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/Nexus.hpp"
#include "Imago/ECS/SparseSet.hpp"
#include "Imago/ECS/SparseSetBase.hpp"
#include "Imago/ECS/SurveyHandle.hpp"

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

// --- SurveyHandle constructed directly, outside of Nexus ---

TEST_CASE("SurveyHandle yields an entity present in every requested pool", "[survey]")
{
  SparseSet<Position> positions;
  SparseSet<Velocity> velocities;

  Entity e = make_entity(0, 0);
  positions.insert(e, Position{ 1, 1 });
  velocities.insert(e, Velocity{ 2, 2 });

  std::array<SparseSetBase*, 2> pools = { &positions, &velocities };
  SurveyHandle<Position, Velocity> survey(pools);

  int count = 0;
  for (Entity found : survey)
  {
    REQUIRE(found == e);
    ++count;
  }
  REQUIRE(count == 1);
}

TEST_CASE("SurveyHandle excludes an entity missing one of the requested components", "[survey]")
{
  SparseSet<Position> positions;
  SparseSet<Velocity> velocities;

  Entity onlyPosition = make_entity(0, 0);
  Entity both = make_entity(1, 0);

  positions.insert(onlyPosition, Position{ 1, 1 });

  positions.insert(both, Position{ 2, 2 });
  velocities.insert(both, Velocity{ 3, 3 });

  std::array<SparseSetBase*, 2> pools = { &positions, &velocities };
  SurveyHandle<Position, Velocity> survey(pools);

  int count = 0;
  for (Entity found : survey)
  {
    REQUIRE(found == both);
    ++count;
  }
  REQUIRE(count == 1);
}

TEST_CASE("SurveyHandle yields nothing when the smallest pool is empty", "[survey]")
{
  SparseSet<Position> positions;
  SparseSet<Velocity> velocities;

  Entity e = make_entity(0, 0);
  positions.insert(e, Position{ 1, 1 }); // velocities pool stays empty

  std::array<SparseSetBase*, 2> pools = { &positions, &velocities };
  SurveyHandle<Position, Velocity> survey(pools);

  int count = 0;
  for (Entity found : survey)
  {
    (void)found;
    ++count;
  }
  REQUIRE(count == 0);
}

TEST_CASE("SurveyHandle correctly selects whichever pool is smaller, regardless of argument order", "[survey]")
{
  SparseSet<Position> positions;
  SparseSet<Velocity> velocities;

  // velocities is deliberately the larger pool, positions the smaller
  Entity match = make_entity(0, 0);
  positions.insert(match, Position{ 1, 1 });
  velocities.insert(match, Velocity{ 1, 1 });

  for (uint32_t i = 1; i <= 5; ++i)
  {
    velocities.insert(make_entity(i, 0), Velocity{});
  }

  std::array<SparseSetBase*, 2> pools = { &positions, &velocities };
  SurveyHandle<Position, Velocity> survey(pools);

  int count = 0;
  for (Entity found : survey)
  {
    REQUIRE(found == match);
    ++count;
  }
  REQUIRE(count == 1); // only 'match' has both, despite velocities having 6 entities total
}

// --- Survey obtained through Nexus::survey<Components...>() ---

TEST_CASE("nexus.survey yields entities with every requested component", "[survey][nexus]")
{
  Nexus nexus;

  Entity a = nexus.create();
  nexus.bind<Position>(a, Position{ 1, 1 });
  nexus.bind<Velocity>(a, Velocity{ 1, 1 });

  Entity b = nexus.create();
  nexus.bind<Position>(b, Position{ 2, 2 }); // no Velocity - should be excluded

  int count = 0;
  for (Entity found : nexus.survey<Position, Velocity>())
  {
    REQUIRE(found == a);
    ++count;
  }
  REQUIRE(count == 1);
}

TEST_CASE("nexus.survey reflects component data correctly through Get", "[survey][nexus]")
{
  Nexus nexus;

  Entity e = nexus.create();
  nexus.bind<Position>(e, Position{ 5, 6 });
  nexus.bind<Velocity>(e, Velocity{ 1, 1 });

  for (Entity found : nexus.survey<Position, Velocity>())
  {
    Position& pos = nexus.get<Position>(found);
    REQUIRE(pos.x == 5);
    REQUIRE(pos.y == 6);
  }
}

TEST_CASE("nexus.survey yields multiple matching entities", "[survey][nexus]")
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

  int count = 0;
  for (Entity found : nexus.survey<Position, Velocity>())
  {
    REQUIRE((found == a || found == b));
    ++count;
  }
  REQUIRE(count == 2);
}

TEST_CASE("nexus.survey for a component type with no pool yet yields nothing", "[survey][nexus]")
{
  Nexus nexus;
  Entity e = nexus.create();
  nexus.bind<Position>(e, Position{}); // Velocity pool never created

  int count = 0;
  for (Entity found : nexus.survey<Position, Velocity>())
  {
    (void)found;
    ++count;
  }
  REQUIRE(count == 0); // ASSUMES the "no pool -> empty survey" resolution discussed earlier
}