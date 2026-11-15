# SparseSet: Component Pooling

Author: Shane David
Type: Documentation
Upload Date: September 17, 2026

## Overview

`SparseSet<T>` is how ImagoEngine stores component data, each component type has a `SparseSet` that holds every Entity’s `T` (component type) in one tightly packed block of memory. It is called a “sparse set” because it’s build from two complementary arrays:

- A **sparse** array, indexed directly by entity index. The sparse array may have gaps as most entities won’t have every component type.
- A **dense** array holds only real data with zero gaps, packed contiguously. This is was actually gets iterated when a system runs.

The sparse array’s only job is to determine whether or not an Entity has a component, and if it does give the index for where it is in the dense array. This results in a single $O(1)$ lookup.

This accomplishes a similar task to a `std::unordered_map<Entity, T>`, however, iterating a component of one type, every frame would mean that each system would have to walk scattered, non-contiguous memory, completely defeating the purpose of an ECS design. `SparseSet` on the other hand guarantees the dense array has no gaps and no scatter allocations, so scanning every `T` in the pool is a straight linear memory walk. This is the entire performance case for a data-oriented ECS design. 

## Basic Usage

NOTE: `SparseSet` is a low-level internal system. Under normal circumstances component binding and unbinding should go through the Nexus, not `SparseSet` directly. That being said, if you as a user desire low level access to Imago’s low level ECS systems you can enable advanced access in order to expose this system. 

```cpp
SparseSet<cTransform> transforms;

Entity player = /* ... */; 

transforms.Insert(player, cTransform{ position, scale, rotation }); 

if (transforms.Has(player))
{
    cTransform& t = transforms.Get(player);
    t.position.x += 5.0f;
}

transforms.Remove(player);
```

## `Insert`, `Replace`, and `TryToGet`

- **`Insert`** - adds a new component. Calling it on an entity that already has one does *not* overwrite, instead it warns and hands back the existing component. Use `Replace` if you actually want to overwrite.
- **`Replace`** - overwrites an existing component in place, with no reordering and no effect on any other entity in the pool. Calling it on an entity with no existing component is a precondition violation (see below).
- **`Get`** - the fast path. Assumes the entity is already known to have the component (typically because a Survey or Bond already filtered to exactly those entities). No safety check in a release build.
- **`TryToGet`** - the safe path. Returns `nullptr` if the entity doesn't have the component, instead of assuming it does. Use this whenever you're not already certain the component exists.

## Removal Order

`Remove` uses a "swap-and-pop" - the last element in the dense array moves into the removed entity's old slot, keeping the array packed with no gaps. This means removing a component can silently change the storage position of some other, unrelated entity's component (specifically, whichever entity was last in the dense array). Nothing about this affects correctness - `Has`/`Get` iteration all still work correctly regardless of position - but it does mean dense-array order is not stable across removals, and should never be relied on to mean anything beyond "packed, no gaps."

## Things to know

- **`Get`** does no bounds checking in release builds. Calling it on an entity without the component is undefined behavior once asserts are compiled out. Always confirm presence first with `Has()`, or by only calling `Get` on entities a Survey/Bond has already filtered, or use`TryToGet` if presence isn't already guaranteed.
- Double-inserting warns rather than crashing or silently overwriting. This is a deliberate choice for `Insert` specifically; `Replace`, `TryToGet`'s missing-component case, and out-of-range indices are all treated as harder precondition violations (asserts), since there's no sensible fallback value to hand back for those.
- A **`SparseSet<T>`** can be accessed through a **`SparseSetBase*`** without knowing what `T` is. This is what lets Nexus store many different component pools behind one uniform pointer type, and is the mechanism Survey and Bond use to query across pools generically.