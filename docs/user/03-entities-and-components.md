# Entities and Components

Create game objects and attach data to them.

---

Use this when you need anything to exist in your game world, a player, a bullet, a camera target, an enemy, a platform, ect. 

**Include:** `Imago/Engine/Scene.hpp` (gives you `Imago::ECS::Nexus`)

---

## Example

```cpp
void on_load() override 
{
	// creates an Entity, exists immediatly
	Entity player = _nexus.create(); 
	
	// bind components to that entity, deferred to end of load
	_nexus.bind<cPosition>(player, {100.f, 50.f}); 
	_nexus.bind<cHealth>(player, {100}); 
}
```

## How it Works

ImagoEngine uses an Entity Component System (ECS) design pattern. This means that an **entity** is nothing but an unsigned integer ID, it has no behavior, and no data. A **component** is a plain struct holding ONLY data. This data would be something like position, or a health value, or an attack stat, ect. You assign data to an entity by **binding** components to it. However, neither of these contain any behavior. Instead, [systems](05-systems.md) iterate through **pools** of components and execute behavior by reading and changing the data. 

All entities and components in a scene live in that scene’s **Nexus**. You can access a Scene’s Nexus through the `_nexus` member in your scene’s `on_load` or through `ctx.nexus` from within a system. 

**Important thing to know:** adding and removing components, and destroying entities is deferred. These changes are structural changes to the component pool, and applying them during iteration of those pools can cause undefined behavior. For that reason, the changes are queued and applied at the end of the frame (or the end of `on_load`), not when you call them. So, remember that adding or removing a component, or destroying an entity, will not happen until the end of the frame, but reading and changing component values happens immediately. 

## Common Tasks

### Defining a component

Components are plain structs. Give fields default values so they are easy to construct. By convention, component names start with `c`

```cpp
struct cVelocity {
	float x = 0.f;
	float y = 0.f; 
}; 
```

Components must be copyable. Avoid members like `std::unique_ptr`.

### Reading and changing a component

```cpp
// see Systems
void Regenerate(SceneContext& ctx, float dt)
{
	// iterate over every entity with cHealth, see Surveys and Bonds
	for (Entity e : ctx.nexus->survey<cHealth>()) {
		
		// get the cHealth component for that entity
		cHealth& hp = ctx.nexus->get<cHealth>(e); 
		
		// edit its value
		hp.current += 1;
	}
}
```

Here, it is safe to use `get` because within a Survey, an entity is guaranteed to have the component. When you are not sure if the entity has a component, use `try_to_get` and do a null check. 

```cpp
if (cHealth* hp = ctx.nexus->try_to_get<cHealth>(target) {
	hp->curent -= 10; 
}
```

### Replacing a component’s whole value

```cpp
// this is not a structural change and therefore happens immediately
ctx.nexus->patch<cPosition>(e, cPosition{0.f, 0.f});
```

### Adding a component

```cpp
// this is a structural change and therefore it is deferred to the end of the frame
ctx.nexus->bind<cVelocity>(e); 
```

### Removing a component

```cpp
// this is a structural change and therefore it is deferred to the end of the frame
ctx.nexus->unbind<cVelocity>(e); 
```

### Destroying an entity

```cpp
// this is a structural change and therefore it is deferred to the end of the frame
**ctx.nexus->destroy(e);** 
```

## API Summary

| Function | What it does | Takes effect |
| --- | --- | --- |
| `create()` | Makes a new entity with no components | Immediately |
| `destroy(e)` | Removes `e` and all its components | End of frame |
| `is_valid(e)` | `true` if `e` hasn't been destroyed | Immediately |
| `bind<T>(e, value)` | Adds component `T` to `e` | End of frame |
| `unbind<T>(e)` | Removes component `T` from `e` | End of frame |
| `get<T>(e)` | Reference to `e`'s `T` (must exist) | Immediately |
| `try_to_get<T>(e)` | Pointer to `e`'s `T`, or `nullptr` | Immediately |
| `has<T>(e)` | `true` if `e` has `T` | Immediately |
| `patch<T>(e, value)` | Overwrites `e`'s existing `T` | Immediately |

## General Guidelines

- After binding a component, `get`, `has`, and `patch` will not see it until the end of the frame. So set the values you want in the `bind` call itself.
- `get` and `patch` require the component to exist. They assert in debug builds, use `try_to_get` or `has` as guards when you are not sure if an entity has a component.
- Binding a component the Entity already has does nothing. If you want to replace a component entirely, use `patch`.
- Queued changes happen in this order: all bind → all unbinds → all destroys.
- **Do not keep references across frames**. A `cHealth&` from `get` can become invalid when other components are added or removed. Call `get` again each time.
- Up to **1,048,575** entities can be alive at once in a scene.

## See Also

[Surveys and Bonds](04-surveys-and-bonds.md)

[Systems](05-systems.md)

[Scenes](02-scenes.md)