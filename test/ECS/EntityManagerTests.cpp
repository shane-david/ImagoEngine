// test/ECS/EntityManagerTests.cpp
#include <catch2/catch_test_macros.hpp>

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/EntityManager.hpp"

using namespace Imago::ECS;

// --- Entity.hpp: pure bit-math, checked at compile time ---

static_assert(GetEntityIndex(MakeEntity(0, 0)) == 0);
static_assert(GetEntityGeneration(MakeEntity(0, 0)) == 0);

static_assert(GetEntityIndex(MakeEntity(42, 3)) == 42);
static_assert(GetEntityGeneration(MakeEntity(42, 3)) == 3);

static_assert(GetEntityIndex(MakeEntity(ENTITY_INDEX_MASK, ENTITY_GENERATION_MASK)) == ENTITY_INDEX_MASK);
static_assert(GetEntityGeneration(MakeEntity(ENTITY_INDEX_MASK, ENTITY_GENERATION_MASK)) == ENTITY_GENERATION_MASK);

static_assert(NULL_ENTITY == 0xFFFFFFFFu);

// --- EntityManager: runtime behavior, checked with Catch2 ---

TEST_CASE("First created entity has index 0 and generation 0", "[entitymanager]")
{
    EntityManager manager;

    Entity e = manager.Create();

    REQUIRE(GetEntityIndex(e) == 0);
    REQUIRE(GetEntityGeneration(e) == 0);
    REQUIRE(manager.IsValid(e));
}

TEST_CASE("Sequential creates get sequential indices", "[entitymanager]")
{
    EntityManager manager;

    Entity first = manager.Create();
    Entity second = manager.Create();
    Entity third = manager.Create();

    REQUIRE(GetEntityIndex(first) == 0);
    REQUIRE(GetEntityIndex(second) == 1);
    REQUIRE(GetEntityIndex(third) == 2);
    REQUIRE(manager.GetAliveCount() == 3);
}

TEST_CASE("Destroy invalidates the entity", "[entitymanager]")
{
    EntityManager manager;
    Entity e = manager.Create();

    manager.Destroy(e);

    REQUIRE_FALSE(manager.IsValid(e));
}

TEST_CASE("Destroyed index is recycled with a bumped generation", "[entitymanager]")
{
    EntityManager manager;
    Entity first = manager.Create();
    manager.Destroy(first);

    Entity second = manager.Create();

    REQUIRE(GetEntityIndex(second) == GetEntityIndex(first));
    REQUIRE(GetEntityGeneration(second) == GetEntityGeneration(first) + 1);
}

TEST_CASE("Stale handle is invalid after its index is recycled", "[entitymanager]")
{
    EntityManager manager;
    Entity first = manager.Create();
    manager.Destroy(first);
    Entity second = manager.Create(); // reuses first's index, bumped generation

    REQUIRE_FALSE(manager.IsValid(first));
    REQUIRE(manager.IsValid(second));
}

TEST_CASE("Free list is FIFO across multiple destroys", "[entitymanager]")
{
    EntityManager manager;
    Entity a = manager.Create(); // index 0
    Entity b = manager.Create(); // index 1
    Entity c = manager.Create(); // index 2

    manager.Destroy(a); // free list: [0]
    manager.Destroy(b); // free list: [0, 1]

    Entity recycledFirst = manager.Create();  // should reuse index 0, not 1
    Entity recycledSecond = manager.Create(); // should reuse index 1

    REQUIRE(GetEntityIndex(recycledFirst) == 0);
    REQUIRE(GetEntityIndex(recycledSecond) == 1);
}

TEST_CASE("NULL_ENTITY is never valid", "[entitymanager]")
{
    EntityManager manager;
    manager.Create(); // give it at least one real entity to compare against

    REQUIRE_FALSE(manager.IsValid(NULL_ENTITY));
}

TEST_CASE("GetAliveCount reflects creates and destroys", "[entitymanager]")
{
    EntityManager manager;

    REQUIRE(manager.GetAliveCount() == 0);

    Entity a = manager.Create();
    Entity b = manager.Create();
    REQUIRE(manager.GetAliveCount() == 2);

    manager.Destroy(a);
    REQUIRE(manager.GetAliveCount() == 1);

    manager.Create(); // recycles a's index
    REQUIRE(manager.GetAliveCount() == 2);
}

TEST_CASE("Destroying an already-invalid entity is a safe no-op", "[entitymanager]")
{
    EntityManager manager;
    Entity e = manager.Create();
    manager.Destroy(e);

    REQUIRE_NOTHROW(manager.Destroy(e)); // double-destroy shouldn't crash or corrupt state
}