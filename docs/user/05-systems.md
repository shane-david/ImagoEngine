# Systems

Contains the logic that reads and changes component to make your game do things.

---

Use this when anything in your game needs to change over time: movement, timers, AI, input handling, scoring, ect. 

Include: `Imago/Engine/Scene.hpp`

## Example

```cpp
void MoveSystem(SceneContext& ctx, float dt) 
{
	// survey everything with position and velocity
	for (Entity e : ctx.nexus->survey<cPosition, cVelocity>()) {
	
		// get access to component data from entities in the survey
		cPosition& pos = ctx.nexus->get<cPosition>(e); 
		const cVelocity& vel = ctx.nexus->get<cVelocity>(e); 
		
		// execute logic with that data 
		pos.x += vel.x * dt; 
		pos,y += vel.y * dt;
	}
}

// register the system in on_load()
_scheduler.register_fixed(MoveSystem); 
```

## How it Works

In ImagoEngine, all game logic lives in **systems**. Systems do not belong to any entity, class, or game object, but instead they loop over all the entities that they care about (usually with a [Survey](04-surveys-and-bonds.md)), and execute game logic by reading and changing their component data. 

There are two main types of systems: **subscribed systems** and **scheduled systems.** Both systems receive `SceneContext& ctx` as their first argument. This is the object by which you can access persistent engine objects (**See SceneContext):**

```cpp
void SubscribedSystem(SceneContext& ctx, const EventType& event); 
```

Subscribed systems execute once when a particular game event occurs and are invoked through the `Broadcaster`. These systems contain logic that does not need to execute every frame, but instead should perform their changes in a single go after a specific event happens. For example, a `PlayerDamage` system should not damage the player every frame, but just once when a different system detects that the player should take damage. **See [Events](06-events.md)** for more information on how to create and emit events in order to invoke subscribed systems. Subscribed systems receive an `EventType* event` as their second argument.  This is a struct that you define which contains any additional data that a subscribed system will need in order to execute. 

```cpp
void ScheduledSystem(SceneContext& ctx, float dt); 
```

Scheduled systems execute every frame or fixed step of the scene that they are registered in. These system contain logic that apply changes to components or have to read component data continuously while the game is executing. For example, an `EnemyAI` script needs to make decisions about component data and move the enemy accordingly every frame, so it would be a scheduled system. Scheduled systems receive `dt` in addition to `ctx`. `dt` is the time step in seconds and should be multiplied to anything that changes over time so that it is not affected by framerate. 

Scheduled systems are registered in you scene’s `on_load`, through the scene’s `_scheduler` member. From then on, the engine calls them for you every frame or fixed step until the scene unloads. 

## Frame Order

Scheduler systems run in this order, each frame:

1. Fixed systems run as many times as needed to keep up with real time. This could be 0, 1, or several times. 
2. Built-in engine systems run. 
3. Your update systems run in you specified order.
4. All queued changes are applied.

## Frame vs Fixed Systems

There are two kinds of scheduled systems, and the only difference is how often they run: 

|  | Update system | Fixed system |
| --- | --- | --- |
| Registered with | `register_update` | `register_fixed` |
| Runs | Once per frame | At a steady rate (60 times per second by default) |
| `dt` is | The real time since the last frame (varies) | Always the same: the fixed timestep |
| Use for | Things the player sees or feels: camera, UI, effects, menus | Things game rules depend on: movement, physics, collision, gameplay timers |

Frame times vary from machine to machine and from moment to moment. For most thing that is fine, because multiplying by `dt` keeps speed correct. However, for physics it is not enough. A jump calculated in big steps on a slow machine comes out slightly lower than a jump in small steps on a fast one, and a long frame can move an object right through a wall without ever touching it. Fixed systems always use the same small step, so the game behaves the same everywhere. 

The general rule is: if logic in a system could change the outcome of the game if ran at a different rate, then it should be a fixed system. For example, a gravity system would have to be fixed because it changes how far and how high things move, so running it at a different rate would change the outcome of the game. On the other hand the camera would be a frame system because it only changes what is drawn so it should be updated every time the screen does. 

## `SceneContext`

Everything a system can reach comes through `ctx`:

| Field | What it is | See |
| --- | --- | --- |
| `ctx.nexus` | This scene's entities and components | {**[Entities and Components](03-entities-and-components.md)** |
| `ctx.broadcaster` | This scene's events | **[Events](06-events.md)** |
| `ctx.input` | The current state of every input action | **[Input](07-input.md)** |
| `ctx.engine` | Switching scenes and quitting | **[Engine Requests](08-engine-requests.md)** |
| `ctx.window` | The game window |  |
| `ctx.assets` | *Not available yet* — always `nullptr` for now |  |

`nexus` and `broadcaster` always belong to the scene the system is running in. `input`, `engine`, and `window` are shared by every scene. 

## Common Tasks

### Registering scheduled systems

Register in `on_load`. Each kind of system runs in the order you register it. 

```cpp
void on_load() override 
{
	_scheduler.register_fixed(GravitySystem); 
	_scheduler.register_fixed(MoveSystem);
	
	_scheduler.register_update(PlayerInputSystem);
	_scheduler.register_update(CameraFollowSystem); 
}
```

Order matters when one system uses another’s results. For example, above, `MoveSystem` needs to be registered second because it uses the velocity `GravitySystem` just changed. 

### Removing a scheduled system

```cpp
_scheduler.unregister(CameraFollowSystem); 
```

This can only be done within the `Scene`, not within systems themselves.

### Turning a behavior on and off during play

Systems can not be registered or unregistered within systems themselves (only within Scenes). Instead a system’s behavior should stop as a response to the component data it is reading. For example an `EnemyAI` system should not execute if the enemy is stunned, so, it should have a guard the skips execution when an enemy is stunned, rather than be unregistered once an enemy is stunned:

```cpp
void EnemyAISystem(SceneContext& ctx, float dt) 
{
    for (Entity e : ctx.nexus->survey<cEnemy, cPosition>()) {
        if (ctx.nexus->has<cStunned>(e)) continue;   // stunned enemies skip their turn
        // normal logic
    }
}
```

### Changing the fixed rate

The fixed rate is 60 fixed steps per second with 8 max fixed steps, but this can be changed to fit your game’s specific needs:

```cpp
_scheduler.set_fixed_timestep(1.0f / 120.0f) // 120 fixed steps per second
_scheduler.set_max_fixed_steps(4); // 4 fixed steps max 
```

## API Summary

| Function | What it does | Notes |
| --- | --- | --- |
| `register_update(system)` | Runs `system` once per frame | Registering twice does nothing |
| `register_fixed(system)` | Runs `system` every fixed step | Registering twice does nothing |
| `unregister(system)` | Removes `system` | Warns if it wasn't registered |
| `set_user_order({...})` | Reorders your update systems | Must list exactly the registered update systems |
| `set_fixed_timestep(seconds)` | Sets the fixed step size | Default `1.0f / 60.0f`; must be > 0 |
| `set_max_fixed_steps(n)` | Caps fixed steps per frame | Default `8`; must be > 0 |

## General Guidelines

- Subscribed systems are called once in response to an event that is emitted through the `Broadcaster`. **See [Events](06-events.md)** for how to create, subscribe, unsubscribe, and emit events.
- Scheduled systems can only be registered and unregistered within scene code, not within systems themselves.
- Avoid `static` variables and globals within systems. Any data that a system needs to remember should be stored in a component that system reads.
- Frame times vary, so anything that changes over time must be multiplied by `dt`, or it will run faster on faster frame rates.
- The engine runs at most `max_fixed_step` per frame (8 by default). If a frame takes longer than that covers (about 133ms on default settings), the extra time is skipped and the game briefly runs in slow motion instead of falling further behind.
- The first frame after a scene loads can have a large `dt` because it includes the time the scene took to load.
- Input works in fixed and frame systems. **See [Input](07-input.md)**

## See Also

[Entites and Components](03-entities-and-components.md)

[Surveys and Bonds](04-surveys-and-bonds.md)

[Scenes](02-scenes.md)

[Events](06-events.md)