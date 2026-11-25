/**
 * @file EntityTests.cpp
 * @brief Tests the Entity packing helpers: make_entity, get_entity_index, get_entity_generation,
 * and the NULL_ENTITY sentinel.
 */

#include <catch2/catch_test_macros.hpp>

#include "Imago/ECS/Entity.hpp"

using namespace Imago::ECS;

TEST_CASE("make_entity packs an index and generation that unpack unchanged", "[Entity]")
{
    const Entity e = make_entity(1234, 56);

    CHECK(get_entity_index(e) == 1234);
    CHECK(get_entity_generation(e) == 56);
}

TEST_CASE("Index and generation occupy separate bits", "[Entity]")
{
    CHECK(make_entity(0, 0) == 0u);
    CHECK(make_entity(1, 0) == 1u);
    CHECK(make_entity(0, 1) == (1u << ENTITY_INDEX_BITS));
    CHECK(ENTITY_INDEX_BITS + ENTITY_GENERATION_BITS == 32u);
}

TEST_CASE("The largest index and generation round-trip", "[Entity]")
{
    const Entity e = make_entity(ENTITY_INDEX_MASK, ENTITY_GENERATION_MASK);

    CHECK(get_entity_index(e) == ENTITY_INDEX_MASK);
    CHECK(get_entity_generation(e) == ENTITY_GENERATION_MASK);
}

TEST_CASE("NULL_ENTITY decodes to the maximum index and generation", "[Entity]")
{
    CHECK(get_entity_index(NULL_ENTITY) == ENTITY_INDEX_MASK);
    CHECK(get_entity_generation(NULL_ENTITY) == ENTITY_GENERATION_MASK);
}

TEST_CASE("Entity helpers are usable at compile time", "[Entity]")
{
    constexpr Entity e = make_entity(7, 3);

    STATIC_REQUIRE(get_entity_index(e) == 7);
    STATIC_REQUIRE(get_entity_generation(e) == 3);
}
