/**
 * @file SparseSetTests.cpp
 * @brief Tests SparseSet, the packed component pool, directly and through SparseSetBase.
 */

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/SparseSet.hpp"
#include "Imago/ECS/SparseSetBase.hpp"

using namespace Imago::ECS;

namespace
{
    struct TestComponent {
        int value = 0;
    };
}

TEST_CASE("insert adds a component and it can be retrieved", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);

    pool.insert(e, TestComponent{42});

    CHECK(pool.has(e));
    CHECK(pool.get(e).value == 42);
    CHECK(pool.get_size() == 1);
}

TEST_CASE("insert on an already-present entity returns the existing component", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);

    pool.insert(e, TestComponent{1});
    TestComponent& existing = pool.insert(e, TestComponent{999}); // should warn, not overwrite

    CHECK(existing.value == 1); // original value preserved
    CHECK(pool.get(e).value == 1);
    CHECK(pool.get_size() == 1); // no duplicate entry
}

TEST_CASE("replace overwrites an existing component in place", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);

    pool.insert(e, TestComponent{1});
    TestComponent& replaced = pool.replace(e, TestComponent{2});

    CHECK(replaced.value == 2);
    CHECK(pool.get(e).value == 2);
    CHECK(pool.get_size() == 1); // still just one entry, not appended
}

TEST_CASE("try_to_get returns nullptr for an entity with no component", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);

    CHECK(pool.try_to_get(e) == nullptr);
}

TEST_CASE("try_to_get returns a valid pointer for an entity with a component", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);
    pool.insert(e, TestComponent{7});

    TestComponent* comp = pool.try_to_get(e);

    REQUIRE(comp != nullptr);
    CHECK(comp->value == 7);
}

TEST_CASE("has correctly reports presence and absence", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity present = make_entity(0, 0);
    Entity absent = make_entity(1, 0);

    pool.insert(present, TestComponent{1});

    CHECK(pool.has(present));
    CHECK_FALSE(pool.has(absent));
    CHECK_FALSE(pool.has(NULL_ENTITY));
}

TEST_CASE("remove invalidates the entity in this pool", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);
    pool.insert(e, TestComponent{1});

    pool.remove(e);

    CHECK_FALSE(pool.has(e));
    CHECK(pool.get_size() == 0);
}

TEST_CASE("Removing a non-last entity performs a correct swap-and-pop", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity a = make_entity(0, 0);
    Entity b = make_entity(1, 0);
    Entity c = make_entity(2, 0);

    pool.insert(a, TestComponent{10});
    pool.insert(b, TestComponent{20});
    pool.insert(c, TestComponent{30});

    pool.remove(b); // middle element - forces the swap path

    CHECK_FALSE(pool.has(b));
    REQUIRE(pool.has(a));
    REQUIRE(pool.has(c));
    CHECK(pool.get(a).value == 10);
    CHECK(pool.get(c).value == 30); // c's data must survive the swap correctly
    CHECK(pool.get_size() == 2);
    CHECK(pool.get_index(c) == 1); // c moved into b's old slot
}

TEST_CASE("Removing the last entity needs no swap", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity a = make_entity(0, 0);
    Entity b = make_entity(1, 0);

    pool.insert(a, TestComponent{10});
    pool.insert(b, TestComponent{20});

    pool.remove(b);

    CHECK_FALSE(pool.has(b));
    REQUIRE(pool.has(a));
    CHECK(pool.get(a).value == 10);
    CHECK(pool.get_index(a) == 0);
    CHECK(pool.get_entities() == std::vector<Entity>{a});
}

TEST_CASE("Removing an already-absent entity is a safe no-op", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);

    REQUIRE_NOTHROW(pool.remove(e)); // never inserted, should just warn and return
    CHECK(pool.get_size() == 0);
}

TEST_CASE("An entity can be inserted again after it was removed", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity a = make_entity(0, 0);
    Entity b = make_entity(1, 0);
    pool.insert(a, TestComponent{1});
    pool.insert(b, TestComponent{2});

    pool.remove(a);
    pool.insert(a, TestComponent{3});

    REQUIRE(pool.has(a));
    CHECK(pool.get(a).value == 3);
    CHECK(pool.get(b).value == 2);
    CHECK(pool.get_size() == 2);
}

TEST_CASE("get_entities returns exactly the entities currently in the pool", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity a = make_entity(0, 0);
    Entity b = make_entity(1, 0);

    pool.insert(a, TestComponent{1});
    pool.insert(b, TestComponent{2});

    const std::vector<Entity>& entities = pool.get_entities();

    REQUIRE(entities.size() == 2);
    CHECK(pool.has(entities[0]));
    CHECK(pool.has(entities[1]));
}

TEST_CASE("get_index reports each entity's packed position", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity a = make_entity(5, 0);
    Entity b = make_entity(2, 0);

    pool.insert(a, TestComponent{1});
    pool.insert(b, TestComponent{2});

    CHECK(pool.get_index(a) == 0);
    CHECK(pool.get_index(b) == 1);
}

TEST_CASE("swap_entities swaps positions and data follows its entity", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity a = make_entity(0, 0);
    Entity b = make_entity(1, 0);
    Entity c = make_entity(2, 0);
    pool.insert(a, TestComponent{10});
    pool.insert(b, TestComponent{20});
    pool.insert(c, TestComponent{30});

    SECTION("two different entities")
    {
        pool.swap_entities(a, c);

        CHECK(pool.get_index(a) == 2);
        CHECK(pool.get_index(c) == 0);
        CHECK(pool.get_entities() == std::vector<Entity>{c, b, a});
        CHECK(pool.get(a).value == 10);
        CHECK(pool.get(c).value == 30);
        CHECK(pool.get(b).value == 20);
    }

    SECTION("an entity with itself is a no-op")
    {
        pool.swap_entities(b, b);

        CHECK(pool.get_entities() == std::vector<Entity>{a, b, c});
        CHECK(pool.get(b).value == 20);
    }
}

TEST_CASE("const get reads the same component", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);
    pool.insert(e, TestComponent{11});

    const SparseSet<TestComponent>& view = pool;

    CHECK(view.get(e).value == 11);
    CHECK(&view.get(e) == &pool.get(e));
}

TEST_CASE("SparseSet can be used polymorphically through SparseSetBase", "[SparseSet]")
{
    SparseSet<TestComponent> pool;
    Entity e = make_entity(0, 0);
    pool.insert(e, TestComponent{5});

    SparseSetBase* base = &pool; // this line alone verifies public inheritance compiles

    CHECK(base->has(e));
    CHECK(base->get_size() == 1);
    CHECK(base->get_entities().size() == 1);
    CHECK(base->get_index(e) == 0);

    base->remove(e);
    CHECK_FALSE(base->has(e));
}
