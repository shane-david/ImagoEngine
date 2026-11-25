/**
 * @file BroadcasterTests.cpp
 * @brief Tests Broadcaster, the event bus: subscribe, emit, unsubscribe, clear, and nested emits.
 */

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "Imago/Engine/Broadcaster.hpp"
#include "Imago/Engine/SceneContext.hpp"

using namespace Imago::Engine;

namespace
{
    using Calls = std::vector<std::string>;

    Calls gCalls;                           ///< what each system saw, in order
    const SceneContext* gSeenCtx = nullptr; ///< the context the last remember_ctx call received

    void clear_log()
    {
        gCalls.clear();
        gSeenCtx = nullptr;
    }

    // test events
    struct EventA {
        int value;
    };
    struct EventB {
        int value;
    };

    // test systems (free functions, since SystemFn is a function pointer)
    void system_a1(SceneContext&, const EventA& e)
    {
        gCalls.push_back("a1:" + std::to_string(e.value));
    }
    void system_a2(SceneContext&, const EventA& e)
    {
        gCalls.push_back("a2:" + std::to_string(e.value));
    }
    void system_a3(SceneContext&, const EventA& e)
    {
        gCalls.push_back("a3:" + std::to_string(e.value));
    }
    void system_b1(SceneContext&, const EventB& e)
    {
        gCalls.push_back("b1:" + std::to_string(e.value));
    }

    void remember_ctx(SceneContext& ctx, const EventA&)
    {
        gSeenCtx = &ctx;
    }

    /// @brief Emits an EventB from inside its own handler through the context's broadcaster.
    void system_a_emits_b(SceneContext& ctx, const EventA& e)
    {
        gCalls.push_back("nest:" + std::to_string(e.value));
        ctx.broadcaster->emit(EventB{e.value + 1});
    }
}

TEST_CASE("Event type ids are stable per type and unique across types", "[Broadcaster]")
{
    EventTypeId a1 = get_event_type_id<EventA>();
    EventTypeId a2 = get_event_type_id<EventA>();
    EventTypeId b = get_event_type_id<EventB>();

    CHECK(a1 == a2);
    CHECK(a1 != b);
}

TEST_CASE("Emit with no subscribers does nothing", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);

    broadcaster.emit(EventA{1});

    CHECK(gCalls.empty());
}

TEST_CASE("A subscribed system is called with the event data", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);

    broadcaster.emit(EventA{5});
    REQUIRE(gCalls == Calls{"a1:5"});

    SECTION("every emit calls it again")
    {
        broadcaster.emit(EventA{7});
        CHECK(gCalls == Calls{"a1:5", "a1:7"});
    }
}

TEST_CASE("Subscribers receive the context the Broadcaster was built with", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(remember_ctx);

    broadcaster.emit(EventA{1});

    CHECK(gSeenCtx == &ctx);
}

TEST_CASE("Events are routed only to systems subscribed to that event type", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventB>(system_b1);

    SECTION("emitting EventA only runs the EventA system")
    {
        broadcaster.emit(EventA{1});
        CHECK(gCalls == Calls{"a1:1"});
    }

    SECTION("emitting EventB only runs the EventB system")
    {
        broadcaster.emit(EventB{2});
        CHECK(gCalls == Calls{"b1:2"});
    }
}

TEST_CASE("Systems run in subscription order", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a2);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventA>(system_a3);

    broadcaster.emit(EventA{3});

    CHECK(gCalls == Calls{"a2:3", "a1:3", "a3:3"});
}

TEST_CASE("Subscribing the same system twice only runs it once per emit", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventA>(system_a1);

    broadcaster.emit(EventA{4});

    CHECK(gCalls == Calls{"a1:4"});
}

TEST_CASE("Unsubscribe removes only the target and keeps the order of the rest", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventA>(system_a2);
    broadcaster.subscribe<EventA>(system_a3);

    broadcaster.unsubscribe<EventA>(system_a2);
    broadcaster.emit(EventA{6});

    CHECK(gCalls == Calls{"a1:6", "a3:6"});
}

TEST_CASE("Unsubscribing a system that is not subscribed is harmless", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);

    SECTION("the event type has no subscribers at all")
    {
        broadcaster.unsubscribe<EventB>(system_b1);
        broadcaster.emit(EventB{1});
        CHECK(gCalls.empty());
    }

    SECTION("the event type has subscribers but not this system")
    {
        broadcaster.subscribe<EventA>(system_a1);
        broadcaster.unsubscribe<EventA>(system_a2);
        broadcaster.emit(EventA{1});
        CHECK(gCalls == Calls{"a1:1"});
    }
}

TEST_CASE("A system can be resubscribed after being unsubscribed", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.unsubscribe<EventA>(system_a1);

    broadcaster.emit(EventA{1});
    REQUIRE(gCalls.empty());

    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.emit(EventA{2});
    CHECK(gCalls == Calls{"a1:2"});
}

TEST_CASE("Clear removes every subscription and the Broadcaster is reusable", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventB>(system_b1);

    broadcaster.clear();
    broadcaster.emit(EventA{1});
    broadcaster.emit(EventB{1});
    REQUIRE(gCalls.empty());

    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.emit(EventA{2});
    CHECK(gCalls == Calls{"a1:2"});
}

TEST_CASE("A system can emit a different event from inside its handler", "[Broadcaster]")
{
    clear_log();
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    ctx.broadcaster = &broadcaster;

    broadcaster.subscribe<EventA>(system_a_emits_b);
    broadcaster.subscribe<EventB>(system_b1);

    broadcaster.emit(EventA{10});

    CHECK(gCalls == Calls{"nest:10", "b1:11"});
}
