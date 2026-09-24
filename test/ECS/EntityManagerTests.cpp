// test/ECS/EntityManagerTests.cpp
#include <catch2/catch_test_macros.hpp>

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/EntityManager.hpp"

using namespace Imago::ECS;

TEST_CASE("First created entity has index 0 and generation 0", "[entitymanager]")
{
    EntityManager manager;

    Entity e = manager.create();

    REQUIRE(get_entity_index(e) == 0);
    REQUIRE(get_entity_generation(e) == 0);
    REQUIRE(manager.is_valid(e));
}

TEST_CASE("Sequential creates get sequential indices", "[entitymanager]")
{
    EntityManager manager;

    Entity first = manager.create();
    Entity second = manager.create();
    Entity third = manager.create();

    REQUIRE(get_entity_index(first) == 0);
    REQUIRE(get_entity_index(second) == 1);
    REQUIRE(get_entity_index(third) == 2);
    REQUIRE(manager.get_alive_count() == 3);
}

TEST_CASE("Destroy invalidates the entity", "[entitymanager]")
{
    EntityManager manager;
    Entity e = manager.create();

    manager.destroy(e);

    REQUIRE_FALSE(manager.is_valid(e));
}

TEST_CASE("Destroyed index is recycled with a bumped generation", "[entitymanager]")
{
    EntityManager manager;
    Entity first = manager.create();
    manager.destroy(first);

    Entity second = manager.create();

    REQUIRE(get_entity_index(second) == get_entity_index(first));
    REQUIRE(get_entity_generation(second) == get_entity_generation(first) + 1);
}

TEST_CASE("Stale handle is invalid after its index is recycled", "[entitymanager]")
{
    EntityManager manager;
    Entity first = manager.create();
    manager.destroy(first);
    Entity second = manager.create(); // reuses first's index, bumped generation

    REQUIRE_FALSE(manager.is_valid(first));
    REQUIRE(manager.is_valid(second));
}

TEST_CASE("Free list is FIFO across multiple destroys", "[entitymanager]")
{
    EntityManager manager;
    Entity a = manager.create(); // index 0
    Entity b = manager.create(); // index 1
    Entity c = manager.create(); // index 2

    manager.destroy(a); // free list: [0]
    manager.destroy(b); // free list: [0, 1]

    Entity recycledFirst = manager.create();  // should reuse index 0, not 1
    Entity recycledSecond = manager.create(); // should reuse index 1

    REQUIRE(get_entity_index(recycledFirst) == 0);
    REQUIRE(get_entity_index(recycledSecond) == 1);
}

TEST_CASE("NULL_ENTITY is never valid", "[entitymanager]")
{
    EntityManager manager;
    manager.create(); // give it at least one real entity to compare against

    REQUIRE_FALSE(manager.is_valid(NULL_ENTITY));
}

TEST_CASE("get_alive_count reflects creates and destroys", "[entitymanager]")
{
    EntityManager manager;

    REQUIRE(manager.get_alive_count() == 0);

    Entity a = manager.create();
    Entity b = manager.create();
    REQUIRE(manager.get_alive_count() == 2);

    manager.destroy(a);
    REQUIRE(manager.get_alive_count() == 1);

    manager.create(); // recycles a's index
    REQUIRE(manager.get_alive_count() == 2);
}

TEST_CASE("Destroying an already-invalid entity is a safe no-op", "[entitymanager]")
{
    EntityManager manager;
    Entity e = manager.create();
    manager.destroy(e);

    REQUIRE_NOTHROW(manager.destroy(e)); // double-destroy shouldn't crash or corrupt state
}