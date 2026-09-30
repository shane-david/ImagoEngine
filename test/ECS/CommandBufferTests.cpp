// test/ECS/CommandBufferTests.cpp
#include <catch2/catch_test_macros.hpp>

#include "Imago/ECS/CommandBuffer.hpp"
#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/Nexus.hpp"

using namespace Imago::ECS;

TEST_CASE("Queued defer_destroy does not take effect until flush", "[commandbuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();

    CommandBuffer commandBuffer(nexus);
    commandBuffer.defer_destroy(e);

    REQUIRE(nexus.is_valid(e)); // still alive - flush() hasn't run yet
}

TEST_CASE("Flush applies a queued defer_destroy", "[commandbuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();

    CommandBuffer commandBuffer(nexus);
    commandBuffer.defer_destroy(e);
    commandBuffer.flush();

    REQUIRE_FALSE(nexus.is_valid(e));
}

TEST_CASE("Flush applies every queued defer_destroy, not just the first", "[commandbuffer]")
{
    Nexus nexus;
    Entity a = nexus.create();
    Entity b = nexus.create();
    Entity c = nexus.create();

    CommandBuffer commandBuffer(nexus);
    commandBuffer.defer_destroy(a);
    commandBuffer.defer_destroy(c);

    commandBuffer.flush();

    REQUIRE_FALSE(nexus.is_valid(a));
    REQUIRE(nexus.is_valid(b));      // never queued - should be untouched
    REQUIRE_FALSE(nexus.is_valid(c));
}

TEST_CASE("Flush clears the queue, so a second flush does nothing new", "[commandbuffer]")
{
    Nexus nexus;
    Entity a = nexus.create();
    Entity b = nexus.create();

    CommandBuffer commandBuffer(nexus);
    commandBuffer.defer_destroy(a);
    commandBuffer.flush();

    REQUIRE_FALSE(nexus.is_valid(a));

    commandBuffer.defer_destroy(b);
    commandBuffer.flush();

    REQUIRE_FALSE(nexus.is_valid(b)); // second flush only applied what was queued after the first
}

TEST_CASE("Flushing an empty queue is a safe no-op", "[commandbuffer]")
{
    Nexus nexus;
    CommandBuffer commandBuffer(nexus);

    REQUIRE_NOTHROW(commandBuffer.flush());
}

TEST_CASE("Queuing the same entity twice and flushing does not crash", "[commandbuffer]")
{
    Nexus nexus;
    Entity e = nexus.create();

    CommandBuffer commandBuffer(nexus);
    commandBuffer.defer_destroy(e);
    commandBuffer.defer_destroy(e); // duplicate - relies on Nexus::defer_destroy's existing safe-no-op behavior

    REQUIRE_NOTHROW(commandBuffer.flush());
    REQUIRE_FALSE(nexus.is_valid(e));
}