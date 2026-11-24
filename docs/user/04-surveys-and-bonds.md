# Surveys and Bonds

Loop over every entity that has a particular set of components.

---

Use this when a system needs to execute logic for every entity with a specified set of components. For example a gravity system would need to apply gravity to every entity with a `cVelocity` and a `cGravity` component. 

**Include: `Imago/Engine/Scene.hpp`**

Most subscribed systems follow the same shape: find all entities with specific components, then read or change those components. ImagoEngine provides 2 different ways to do that:

- **Surveys:** quick, flexible queries that you can create whenever you need them. Surveys should be used by default. See **Surveys**
- **Bonds:** permanent, pre=sorted groups for loops that run every frame over many entities. Iteration is faster but they come with far more restrictions and require more thought. See **Bonds**

## Surveys

### Example

```cpp
void GravitySystem(SceneContext& ctx, float dt)
{
	// iterate through all entities with cVelocity and cGravity
	for (Entity e : ctx.nexus->survey<cVelocity, cGravity>()) {
	
		// get velocity component to be edited
		cVelocity& vel = ctx.nexus->get<cVelocity>(e); 
		
		// get gravity component to be read
		const cGravity& grav = ctx.nexus->get<cGravity>(e); 
		
		// execute logic
		vel.y += grav.strength * dt; 
	}
}
```

NOTE: `get` is always safe inside the loop for a Survey over Bond over specific component types because every entity the Survey or Bond gives you is guaranteed to have every listed component. 

### How it Works

A survey finds every entity that has **all** of the component types you specify. You create one with:

```cpp
ctx.nexus->survey<...>
```

and loop over it with a range-based `for` loop. Each pass gives you an `Entity`, and you use `get` to reach its components. 

Surveys are cheap to create, so make a new one each time you need it; this is usually once per system call. 

### Common Tasks

#### Surveying a single component

```cpp
for (Entity e : ctx.nexus->survey<cHealth>()) {
	if (ctx.nexus->get<cHealth>(e).current <= 0) {
		ctx,nexus->destroy(e); 
	}
}
```

#### Using a component you did not survey for

If you need a component only some of the entitles have, do not add it to the survey and check inside the loop. 

```cpp
for (Entity e : ctx.nexus->survey<cPosition, cEnemy>()) {
	if (cShield* shield = ctx.nexus->try_to_get<cShield>(e) {
		// logic for only sheiled enemies
	}
	
	// logic for rest of enemies 
}
```

#### Adding, removing, and destroying during a loop

All three structural changes are safe inside a survey loop. They’re queued and applied at the end of the frame, so the loop always finishes over an unchanged set of entities:

```cpp
for (Entity e : ctx.nexus->survey<cLifetime>()) {
	cLilfetime& lifetime = ctx.nexus->get<cLifetime>(e); 
	life.secondsLeft -= dt; 
	if (life.secondsLeft <= 0.f) {
		ctx.nexus->destroy(e); 
	}
}
```

#### Surveying during scene set up

Survey work in `on_load` too. Use `_nexus` member instead of `ctx.nexus`

```cpp
for (Entity e : _nexus.survey<cSpawnPoint>()) {
	// spawn logic 
}
```

Remember that entities and components you bound earlier in the same `on_load` are not applied yet, so the survey won’t find them. 

### API Summary

| Function | What it does | Notes |
| --- | --- | --- |
| `survey<A, B, ...>()` | Returns a survey of every entity with all of `A`, `B`, ... | Loop it with `for (Entity e : ...)` |

### General Guidelines

- Entities do not come back in creation order, and the order can and will change as components are added and removed. Do not write logic that depends on creation order.
- Structural changes (binds, unbinds, and destroys) will not show up until next frame.
- Do not store a survey or anything you get from looping over it in a component, a static variable, or a member. It refers to the scene’s internal storage and is not valid after the scene unloads. It is cheap and much safer to make a new survey every time.
- A survey with nothing in it will not compile, list at least one component.
- It is safe to survey a component that is not bound to any entity, the loop will simply not execute.
- A survey checked every entity in the smallest of the listed component pools against the others. This is still extremely fast for most loops. However, if a multi-component loop runs every frame over thousands of entites, consider a `Bond`

## Bonds

### Example

 The first `bond<...>()` call creates the bond and sorts the existing entities which takes some time. Every later call with the same component types just returns the existing bond, so it is cheap to call every frame. Therefore, you should create the bond once when the scene loads, then use it in a system: 

```cpp
// in your scene's on_load
_nexus.bond<cBoid, cFlocking>(); 

// in the bird flocking system
void BirdFlockingSystem(SceneContext& ctx, float dt) 
{
	// get a view of the bond (since it was created in on_load)
	for (Entity e : ctx.nexus->bond<cBoid, cFlocking>()) {
		// execute bird flocking logic 
	}
}
```

### How it Works

A bond has the same use-case as a survey but is a **permanent** group of component types. When you create one, the engine rearranges those components’ storage so that every entity having **all** of them sits together at the front of their respective pool, in the same order. It then ensures that this ordering remains as components are added and removed. 

Because all matching entities are grouped together for every component pool, unlike surveys, looping over a bond never has to check whether or not the entity is in every pool. It simply visits exactly the entities you want to check and does nothing else. That makes bonds the fastest way to loop over entities with specific components, with a couple important caveats:  

Since bonds rearrange storage, each component type can only be in one bond per scene. Two bonds sharing the same component type would fight over ordering. Asking for a bond that overlaps on an existing one crashes the program:

```cpp
// crash
_nexus.bond<cBoid, cFlocking>(); 
_nexus.bond<cBoid, cHealth>(); 
```

Similarly, since binds, unbinds, and destroys could change which entities should belong in a bond, these calls become more expensive since it has to notify the Bond about structural changes so it can maintain its order. 

### Common Tasks

Because each component type can be in only one bond per scene, which pools you decide to bond should be carefully considered. Use a survey be default and reach for a bond when **all** of these are true:

- The loop runs every frame (or fixed step) in all cases for that scene.
- It covers a large quantity of entities.
- The components are rarely added or removed.

Common candidates for bonds are large component sets that define **what an entity is** and stay with it for its whole life:

| Bond | Example use |
| --- | --- |
| `bond<cBoid, cFlocking>()` | Flocking birds or fish |
| `bond<cCrop, cGrowth>()` | Every growing plant in a farming game |
| `bond<cTurret, cTargeting>()` | Every turret choosing a target |

However, choosing whether or not to use bond is not always straight forward. Particles are commonly considered for bonds because they are usually a large amount of entities. However, particles are usually created and destroyed constantly, which works against a bond, meaning that a survey might be just as fast. Use surveys by default and weigh all of the advantages and disadvantages of using a bond in your game before deciding to use it. 

### API Summary

| Function | What it does | Notes |
| --- | --- | --- |
| `bond<A, B, ...>()` | Creates the bond the first time; returns it after that | Loop it with `for (Entity e : ...)` |

### General Guidelines

- Each component type can only be in one bond per scene. Asking for a bond that overlaps an existing one crashes the program.
- Bonds cannot be removed, once created a bond last until the scene unloads. Similarly, bonding should be done in `on_load` because the initial creation of a bond could take some time. Plan your bonds when the scene loads.
- Some engine-defined component types are bonded by the engine itself. You cannot include those in your own bonds. <!— TODO: list the component types the engine’s core systems bond by default once those systems are written.
- Entities do not come back in creation order, and the order can and will change as components are added and removed. Do not write logic that depends on creation order.
- Structural changes (binds, unbinds, and destroys) will not show up until next frame.
- Do not store a survey or anything you get from looping over it in a component, a static variable, or a member. It refers to the scene’s internal storage and is not valid after the scene unloads. It is cheap and much safer to make a new survey every time.

## See Also

[Entities and Components](03-entities-and-components.md)

[Systems](05-systems.md)

[Scenes](02-scenes.md)