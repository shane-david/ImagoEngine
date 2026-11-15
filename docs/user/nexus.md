# Nexus

Author: Shane David
Type: Documentation
Upload Date: September 21, 2026

## Overview

`Nexus` is considered the ECS world, it is the single caller for everything ECS-related in ImagoEngine. It creates every Entity, binds/unbinds every component, and every system uses it to write and read component data. It handles Entities and components by internally owning an `EntityManager` instance and a pool of `SparseSet<T>` for each component type. The standard user will never `EntityManager` or `SparseSet<T>` directly because all Entity and component interaction is routed through the `Nexus`. The `Nexus` will be the most used class in standard development and should be mastered by every user who wishes to use ImagoEngine. 

<aside>

NOTE: In real gameplay code you will not construct the Nexus. The Nexus will be passed to each system through a `SceneContext` reference. You can than access the Nexus via `ctx.nexus`.

</aside>

## Creating and Destroying Entities

```cpp
Entity player = nexus->Create(); 

nexus->Destroy(player); 
```

`Destroy` does more than `EntityManager::Destroy` for the Nexus. The Nexus also removes the Entity from every component pool so that you do not need to manually unbind every component before destroying an entity. 

## Binding Components

```cpp
nexus->Bind<cTransform>(player, cTransform{ position, scale, rotation }); 
```

`Bind` attaches a new component of type `T` (should be a component) to an Entity. Pool are created lazily so if a component that does not yet have registered pool is bound, the pool will be created. There is no setup or registration step for a new component type, the component must simply be defined and it can be bound. 

NOTE: Binding a component the entity already has will not overwrite it. It will flag a warning and return the same component it originally had. If you wish to entirely replace an existing component use `Patch` instead:

```cpp
nexus->Patch<cTransform>(player, cTransform{ newPosition, newScale, newRotation }); 
```

## Unbinding Components

```cpp
nexus->Unbind<cCollider>(player); 
```

This just removes the one specified component and leaves every other component on the Entity untouched.. This is safe to call even if the entity does not have the component in the first place because`Unbind` will recognize that and do nothing. 

## Reading Components

There are two ways to read a component back, depending on how sure you are that the component exists. Both of the methods return either a reference or a pointer to the component so component data can be edited directly. This is preferred to `Patch` if the component data changing relies on the previous component data (increasing velocity, changing position, ect.) 

 If you are certain that the specified Entity has the specified component (for example you are using a Survey/Bond) you can use `Get`:

```cpp
cTransform& t = nexus->Get<cTransform>(player); 
t.position.x += 5.0f; 
```

If you are not certain that the specified Entity has the specified component, you must use `TryToGet` as `Get` will crash the program if it is called on an Entity that does not have the specified component.

```cpp
if (cTransform* t = nexus->TryToGet<cTransform>(player)) 
{
	t->position.x += 5.0f; 
}
```

NOTE: `TryToGet` returns a pointer instead of a reference so that it can return `nullptr` if there is no such component instead of crashing the program. 

You can use `Has` if you just need to know whether or not an Entity has a component. However, this only returns a Boolean so if you need to component data `TryToGet` is preferred.

```cpp
if (nexus->Has<cRigidBody>(player)) 
{
	// perform logic
}
```

## Things to Know

- `Get` assumes the component already exists. Calling it on an Entity that does not have that component will either crash the program or cause undefined behavior in release builds. Use `TryToGet` or guard with `Has` first if you are not certain an Entity has the desired component.
- `Nexus` cannot be copied. There should only ever be one ECS world in a Scene. The compiler enforces this directly.
- `EntityManager` and the individual `SparseSet<T>` pools are internal. The standard user should never reach past the `Nexus` for anything.

## See Also

- `Entity.hpp` / `EntityManager`
- `SparseSet<T>`
- `SparseSetBase`