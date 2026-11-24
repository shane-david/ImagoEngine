# Events

Let subscribed systems react to things that happen without knowing about each other. 

---

Use this when one or multiple parts of your game need to respond to something another part did: a player dying, and item being picked up, a level being finished, ect. 

Include: `Imago/Engine/Scene.hpp`

---

## Example

```cpp
// event declaration
struct PlayerDied { Entity player; }; 

// subscribed system
void OnPlayerDied(SceneContext& ctx, const PlayerDied& event) 
{
	ctx.nexus->destroy(event.player); 
	ctx.engine->switch_scene("GameOver"); 
}

// subscription in on_load
_broadcaster.subscribe<PlayerDied>(OnPlayerDied); 

// in whatever system detects the player's health hitting 0 
ctx.broadcaster->emit(PlayerDied{player}); 
```

## How it Works

An **event** is a plain struct describing (providing data for) something that happened. A system **emits** an event, and every system **subscribed** to that event type is called with it right away. Systems that subscribe to events (instead of being called every frame) are called **subscribed systems** and have the following signature:

```cpp
void SystemName(SceneContext& ctx, const EventType& event); 
```

See [Systems](05-systems.md)

The system that emits the event does not know who is listening, and the listeners do not know who emitted it. That keeps your system decoupled. You can add a new reaction to an event without touching the code that emits it. 

Each scene has its own **`Broadcaster`** that handles this. In `on_load`, use the scene’s `_broadcaster` member to subscribe to events, in a system, use `ctx.broadcaster` to emit events. 

## Common Tasks

### Defining an event

Events are plain data structs. Put in whatever data the listeners will need to know:

```cpp
struct DamageTaken {
	Entity target; 
	int amount; 
	Entity source; 
}; 
```

NOTE: If subscribed systems that do not need any data all simply did not receive an event struct, the `Broadcaster` would have know way of knowing which event is which. For this reason, even when the subscribed system does not need any data, an event struct must still be defined, it can be left empty.

```cpp
struct LevelComplete {}; 
struct PauseToggled {}; 
```

### Subscribing

In most cases subscribe in your scene’s `on_load` so your listeners are ready before the first frame. Subscribe with the syntax `_broadcaster.subscribe<EventType>(SubscribedSystemName)`:

```cpp
void on_load() override 
{
	_broadcaster.subscribe<PlayerDied>(OnPlayerDied); 
}
```

### Emitting

Emit from any system, with `ctx.broadcaster->emit(EventType{data})`. The event type is worked out from what you pass in:

```cpp
void HealthSystem(SceneContext& ctx, float dt) 
{
	for (Entity e : ctx.nexus->survey<cHealth>()) {
		if (ctx.nexus->get<cHealth>(e).current <= 0) {
			ctx.broadcaster->emit(EntityDied{e}); 
		}
	}
}
```

### Several listeners for one event

Any number of systems can subscribe to the same event. They are called in the same order they subscribed:

```cpp
_broadcaster.subscribe<DamageTaken>(ApplyKnockback);
_broadcaster.subscribe<DamageTaken>(StartHitFlash);
_broadcaster.subscribe<DamageTaken>(UpdateHealthUI); 
```

### Emitting from a listener

A listener can emit another event. It is handled immediately, before the first listener continues:

```cpp
void AddScore(SceneContext& const EnemyDied& event) 
{
	if (event.score >= 100) {
		ctx.broadcaster->emit(PlayerWin{event.score}); 
	}
}
```

### Unsubscribing

```cpp
_broadcaster.unsubscribe<PlayerDied>(OnPlayerDied); 
```

## API Summary

| Function | What it does | Notes |
| --- | --- | --- |
| `subscribe<T>(system)` | Calls `system` whenever a `T` is emitted | Subscribing twice does nothing |
| `unsubscribe<T>(system)` | Stops calling `system` for `T` | Warns if it wasn't subscribed |
| `emit(event)` | Calls every subscriber to the event's type | Immediate; in subscription order |
| `clear()` | Removes every subscriber |  |

## General Guidelines

- `emit` does not return until every listener has run, meaning events are handled immediately, event if an event is called in the middle of a survey loop.
- Adding, removing, and destroying in a listener is still safe as they will still be called with the Nexus and therefore will be deferred until the end of  the frame.
- Listeners run in the order they subscribed.
- Do not keep a pointer or reference to the event, it only exists during the `emit` call.
- Events emitted in one scene are never heard by listeners in another, even if it is the same event, because each scene has its own `Broadcaster`,

## See Also

[Systems](05-systems.md)

[Entites and Components](03-entities-and-components.md)

[Engine Requests](08-engine-requests.md)