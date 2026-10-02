#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "Imago/Engine/Broadcaster.hpp"

// SceneContext is only forward declared in Broadcaster.hpp, so the test defines a
// minimal stand-in. Systems record what they saw into `calls`.
namespace Imago::Engine
{
    struct SceneContext
    {
        std::vector<std::string> calls;
        Broadcaster* broadcaster = nullptr; // only used by the nested emit test
    };
}

using namespace Imago::Engine;

namespace
{
    // test events
    struct EventA { int value; };
    struct EventB { int value; };

    // test systems (free functions, since SystemFn is a function pointer)
    void system_a1(SceneContext& ctx, const EventA& e) { ctx.calls.push_back("a1:" + std::to_string(e.value)); }
    void system_a2(SceneContext& ctx, const EventA& e) { ctx.calls.push_back("a2:" + std::to_string(e.value)); }
    void system_a3(SceneContext& ctx, const EventA& e) { ctx.calls.push_back("a3:" + std::to_string(e.value)); }
    void system_b1(SceneContext& ctx, const EventB& e) { ctx.calls.push_back("b1:" + std::to_string(e.value)); }

    // emits an EventB from inside its own handler
    void system_a_emits_b(SceneContext& ctx, const EventA& e)
    {
        ctx.calls.push_back("nest:" + std::to_string(e.value));
        ctx.broadcaster->emit(EventB{ e.value + 1 });
    }

    using Calls = std::vector<std::string>;
}

TEST_CASE("Event type ids are stable per type and unique across types", "[Broadcaster]")
{
    EventTypeId a1 = get_event_type_id<EventA>();
    EventTypeId a2 = get_event_type_id<EventA>();
    EventTypeId b  = get_event_type_id<EventB>();

    REQUIRE(a1 == a2);
    REQUIRE(a1 != b);
}

TEST_CASE("Emit with no subscribers does nothing", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);

    broadcaster.emit(EventA{ 1 });

    REQUIRE(ctx.calls.empty());
}

TEST_CASE("A subscribed system is called with the event data", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);

    broadcaster.emit(EventA{ 5 });
    REQUIRE(ctx.calls == Calls{ "a1:5" });

    SECTION("every emit calls it again")
    {
        broadcaster.emit(EventA{ 7 });
        REQUIRE(ctx.calls == Calls{ "a1:5", "a1:7" });
    }
}

TEST_CASE("Events are routed only to systems subscribed to that event type", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventB>(system_b1);

    SECTION("emitting EventA only runs the EventA system")
    {
        broadcaster.emit(EventA{ 1 });
        REQUIRE(ctx.calls == Calls{ "a1:1" });
    }

    SECTION("emitting EventB only runs the EventB system")
    {
        broadcaster.emit(EventB{ 2 });
        REQUIRE(ctx.calls == Calls{ "b1:2" });
    }
}

TEST_CASE("Systems run in subscription order", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a2);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventA>(system_a3);

    broadcaster.emit(EventA{ 3 });

    REQUIRE(ctx.calls == Calls{ "a2:3", "a1:3", "a3:3" });
}

TEST_CASE("Subscribing the same system twice only runs it once per emit", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventA>(system_a1);

    broadcaster.emit(EventA{ 4 });

    REQUIRE(ctx.calls == Calls{ "a1:4" });
}

TEST_CASE("Unsubscribe removes only the target and keeps the order of the rest", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventA>(system_a2);
    broadcaster.subscribe<EventA>(system_a3);

    broadcaster.unsubscribe<EventA>(system_a2);
    broadcaster.emit(EventA{ 6 });

    REQUIRE(ctx.calls == Calls{ "a1:6", "a3:6" });
}

TEST_CASE("Unsubscribing a system that is not subscribed is harmless", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);

    SECTION("the event type has no subscribers at all")
    {
        broadcaster.unsubscribe<EventB>(system_b1);
        broadcaster.emit(EventB{ 1 });
        REQUIRE(ctx.calls.empty());
    }

    SECTION("the event type has subscribers but not this system")
    {
        broadcaster.subscribe<EventA>(system_a1);
        broadcaster.unsubscribe<EventA>(system_a2);
        broadcaster.emit(EventA{ 1 });
        REQUIRE(ctx.calls == Calls{ "a1:1" });
    }
}

TEST_CASE("A system can be resubscribed after being unsubscribed", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.unsubscribe<EventA>(system_a1);

    broadcaster.emit(EventA{ 1 });
    REQUIRE(ctx.calls.empty());

    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.emit(EventA{ 2 });
    REQUIRE(ctx.calls == Calls{ "a1:2" });
}

TEST_CASE("Clear removes every subscription and the Broadcaster is reusable", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.subscribe<EventB>(system_b1);

    broadcaster.clear();
    broadcaster.emit(EventA{ 1 });
    broadcaster.emit(EventB{ 1 });
    REQUIRE(ctx.calls.empty());

    broadcaster.subscribe<EventA>(system_a1);
    broadcaster.emit(EventA{ 2 });
    REQUIRE(ctx.calls == Calls{ "a1:2" });
}

TEST_CASE("A system can emit a different event from inside its handler", "[Broadcaster]")
{
    SceneContext ctx;
    Broadcaster broadcaster(ctx);
    ctx.broadcaster = &broadcaster;

    broadcaster.subscribe<EventA>(system_a_emits_b);
    broadcaster.subscribe<EventB>(system_b1);

    broadcaster.emit(EventA{ 10 });

    REQUIRE(ctx.calls == Calls{ "nest:10", "b1:11" });
}