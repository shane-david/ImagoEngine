# Entities and Generational Indices

Author: Shane David

# Entities

### What is an Entity?

An `Entity` in ImagoEngine is not an object, it is just a number. Specifically it is an alias for a `uint32_t` that packs two pieces of information together:

- **Index** (20 bits) - which slot the Entity occupies
- **Generation** (12 bits) - how many times that slot has been destroyed or reused

Entities carry no data of their own. They are keys you use to look up components stored elsewhere. Entities existing like this is the foundation of ImagoEngine’s ECS design: an Entity is just an id, nothing more. 

### Why generations?

For optimization and memory purposes Entity indices are reused. However this can cause issues: since Entities are just numbers there is no way to distinguish between a fresh Entity and a reused Entity. For example, consider an Entity that exists at index 5. This entity was handed to three different systems and then eventually destroyed. Later, that index is reused and a new, completely unrelated Entity exists at index 5. Now those three systems are silently pointing at an unrelated Entity now that index 5 is valid again. 

Generations solve this issue. Every time an index is destroyed and recycled its generation increments. Now, an old Entity handle carries the old generation that was packed into it. So, when a new Entity is created, systems that are trying to access that Entity cannot because even though the index matches, `IsValid()` sees the generation mismatch and correctly reports the old Entity as dead. 

### Using the `EntityManager`

NOTE: `EntityManager` is a low-level internal system. Under normal circumstances entity creation and destruction should go through the Nexus, not `EntityManager` directly. That being said, if you as a user desire low level access to Imago’s low level ECS systems you can enable advanced access in order to expose this system. 

```cpp
EntityManager manager; 

// player is now a valid entity ready to have components bound to it
Entity player = manager.Create(); 

// player's underlying slot is freed for reuse and the old player handle is stale
manager.Destroy(player); 

manager.IsValid(player); // false, even though the index may be bound to an entity,
												 // the generations will not match
 
```

### Additional Notes

- `NULL_ENTITY` is a reserved sentinel value meaning “no entity.” It is never returned by a successful `Create()` call, and `IsValid(NULL_ENTITY)` always returns `false`.
- There is a maximum entity count supported by ImagoEngine, ($2^{20}$ - roughly 1 million), tied to the 20 bits reserved for the index. This ceiling is enforced in development builds.
- Generations can theoretically wrap after 4096 destroy/recycle cycles on the exact same index. This is an extremely rare scenario in practice and is actively monitored during development.
- `EntityManager` is not thread safe, all entity creation/destruction should happen from a single thread.

### See Also

`Entity.hpp`

`EntityManager.hpp`