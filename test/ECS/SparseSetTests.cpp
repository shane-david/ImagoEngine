// test/ECS/SparseSetTests.cpp
#include <catch2/catch_test_macros.hpp>

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/SparseSet.hpp"
#include "Imago/ECS/SparseSetBase.hpp"

using namespace Imago::ECS;

namespace
{
  struct TestComponent
  {
    int value = 0;
  };
}

TEST_CASE("insert adds a component and it can be retrieved", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);

  pool.insert(e, TestComponent{ 42 });

  REQUIRE(pool.has(e));
  REQUIRE(pool.get(e).value == 42);
  REQUIRE(pool.get_size() == 1);
}

TEST_CASE("insert on an already-present entity returns the existing component", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);

  pool.insert(e, TestComponent{ 1 });
  TestComponent& existing = pool.insert(e, TestComponent{ 999 }); // should warn, not overwrite

  REQUIRE(existing.value == 1);   // original value preserved
  REQUIRE(pool.get(e).value == 1);
  REQUIRE(pool.get_size() == 1);  // no duplicate entry
}

TEST_CASE("replace overwrites an existing component in place", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);

  pool.insert(e, TestComponent{ 1 });
  pool.replace(e, TestComponent{ 2 });

  REQUIRE(pool.get(e).value == 2);
  REQUIRE(pool.get_size() == 1); // still just one entry, not appended
}

TEST_CASE("try_to_get returns nullptr for an entity with no component", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);

  REQUIRE(pool.try_to_get(e) == nullptr);
}

TEST_CASE("try_to_get returns a valid pointer for an entity with a component", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);
  pool.insert(e, TestComponent{ 7 });

  TestComponent* comp = pool.try_to_get(e);

  REQUIRE(comp != nullptr);
  REQUIRE(comp->value == 7);
}

TEST_CASE("has correctly reports presence and absence", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity present = make_entity(0, 0);
  Entity absent = make_entity(1, 0);

  pool.insert(present, TestComponent{ 1 });

  REQUIRE(pool.has(present));
  REQUIRE_FALSE(pool.has(absent));
  REQUIRE_FALSE(pool.has(NULL_ENTITY));
}

TEST_CASE("remove invalidates the entity in this pool", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);
  pool.insert(e, TestComponent{ 1 });

  pool.remove(e);

  REQUIRE_FALSE(pool.has(e));
  REQUIRE(pool.get_size() == 0);
}

TEST_CASE("Removing a non-last entity performs a correct swap-and-pop", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity a = make_entity(0, 0);
  Entity b = make_entity(1, 0);
  Entity c = make_entity(2, 0);

  pool.insert(a, TestComponent{ 10 });
  pool.insert(b, TestComponent{ 20 });
  pool.insert(c, TestComponent{ 30 });

  pool.remove(b); // middle element - forces the swap path

  REQUIRE_FALSE(pool.has(b));
  REQUIRE(pool.has(a));
  REQUIRE(pool.has(c));
  REQUIRE(pool.get(a).value == 10);
  REQUIRE(pool.get(c).value == 30); // c's data must survive the swap correctly
  REQUIRE(pool.get_size() == 2);
}

TEST_CASE("Removing an already-absent entity is a safe no-op", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);

  REQUIRE_NOTHROW(pool.remove(e)); // never inserted, should just warn and return
  REQUIRE(pool.get_size() == 0);
}

TEST_CASE("get_entities returns exactly the entities currently in the pool", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity a = make_entity(0, 0);
  Entity b = make_entity(1, 0);

  pool.insert(a, TestComponent{ 1 });
  pool.insert(b, TestComponent{ 2 });

  const std::vector<Entity>& entities = pool.get_entities();

  REQUIRE(entities.size() == 2);
  REQUIRE(pool.has(entities[0]));
  REQUIRE(pool.has(entities[1]));
}

TEST_CASE("SparseSet can be used polymorphically through SparseSetBase", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = make_entity(0, 0);
  pool.insert(e, TestComponent{ 5 });

  SparseSetBase* base = &pool; // this line alone verifies public inheritance compiles

  REQUIRE(base->has(e));
  REQUIRE(base->get_size() == 1);
  REQUIRE(base->get_entities().size() == 1);

  base->remove(e);
  REQUIRE_FALSE(base->has(e));
}