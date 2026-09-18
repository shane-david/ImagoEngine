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

TEST_CASE("Insert adds a component and it can be retrieved", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);

  pool.Insert(e, TestComponent{ 42 });

  REQUIRE(pool.Has(e));
  REQUIRE(pool.Get(e).value == 42);
  REQUIRE(pool.GetSize() == 1);
}

TEST_CASE("Insert on an already-present entity returns the existing component", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);

  pool.Insert(e, TestComponent{ 1 });
  TestComponent& existing = pool.Insert(e, TestComponent{ 999 }); // should warn, not overwrite

  REQUIRE(existing.value == 1);   // original value preserved
  REQUIRE(pool.Get(e).value == 1);
  REQUIRE(pool.GetSize() == 1);   // no duplicate entry
}

TEST_CASE("Replace overwrites an existing component in place", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);

  pool.Insert(e, TestComponent{ 1 });
  pool.Replace(e, TestComponent{ 2 });

  REQUIRE(pool.Get(e).value == 2);
  REQUIRE(pool.GetSize() == 1); // still just one entry, not appended
}

TEST_CASE("TryToGet returns nullptr for an entity with no component", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);

  REQUIRE(pool.TryToGet(e) == nullptr);
}

TEST_CASE("TryToGet returns a valid pointer for an entity with a component", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);
  pool.Insert(e, TestComponent{ 7 });

  TestComponent* comp = pool.TryToGet(e);

  REQUIRE(comp != nullptr);
  REQUIRE(comp->value == 7);
}

TEST_CASE("Has correctly reports presence and absence", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity present = MakeEntity(0, 0);
  Entity absent = MakeEntity(1, 0);

  pool.Insert(present, TestComponent{ 1 });

  REQUIRE(pool.Has(present));
  REQUIRE_FALSE(pool.Has(absent));
  REQUIRE_FALSE(pool.Has(NULL_ENTITY));
}

TEST_CASE("Remove invalidates the entity in this pool", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);
  pool.Insert(e, TestComponent{ 1 });

  pool.Remove(e);

  REQUIRE_FALSE(pool.Has(e));
  REQUIRE(pool.GetSize() == 0);
}

TEST_CASE("Removing a non-last entity performs a correct swap-and-pop", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity a = MakeEntity(0, 0);
  Entity b = MakeEntity(1, 0);
  Entity c = MakeEntity(2, 0);

  pool.Insert(a, TestComponent{ 10 });
  pool.Insert(b, TestComponent{ 20 });
  pool.Insert(c, TestComponent{ 30 });

  pool.Remove(b); // middle element - forces the swap path

  REQUIRE_FALSE(pool.Has(b));
  REQUIRE(pool.Has(a));
  REQUIRE(pool.Has(c));
  REQUIRE(pool.Get(a).value == 10);
  REQUIRE(pool.Get(c).value == 30); // c's data must survive the swap correctly
  REQUIRE(pool.GetSize() == 2);
}

TEST_CASE("Removing an already-absent entity is a safe no-op", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);

  REQUIRE_NOTHROW(pool.Remove(e)); // never inserted, should just warn and return
  REQUIRE(pool.GetSize() == 0);
}

TEST_CASE("GetEntities returns exactly the entities currently in the pool", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity a = MakeEntity(0, 0);
  Entity b = MakeEntity(1, 0);

  pool.Insert(a, TestComponent{ 1 });
  pool.Insert(b, TestComponent{ 2 });

  const std::vector<Entity>& entities = pool.GetEntities();

  REQUIRE(entities.size() == 2);
  REQUIRE(pool.Has(entities[0]));
  REQUIRE(pool.Has(entities[1]));
}

TEST_CASE("SparseSet can be used polymorphically through SparseSetBase", "[sparseset]")
{
  SparseSet<TestComponent> pool;
  Entity e = MakeEntity(0, 0);
  pool.Insert(e, TestComponent{ 5 });

  SparseSetBase* base = &pool; // this line alone verifies public inheritance compiles

  REQUIRE(base->Has(e));
  REQUIRE(base->GetSize() == 1);
  REQUIRE(base->GetEntities().size() == 1);

  base->Remove(e);
  REQUIRE_FALSE(base->Has(e));
}