/**
 *  @file EntityManager.hpp
 *  @brief Owns the lifetime of every Entity in the engine, it creates, destroys, and valildates every entity. Handles recyclying freed indices by bumping their generation to invalidate stale Entity handles. 
 */
# pragma once 

#include <cstdint> // for uint32_t
#include <vector>  // for holding the Entity generations
#include <queue>   // for holding free indices

#include "Imago/ECS/Entity.hpp" // for Entity alias and bitwise math


namespace Imago::ECS 
{

    /**
     * @brief Creates, destroys, and validates Entity handles.
     *
     * EntityManager is the single source of truth for which entities currently exist.
     * Destroyed indices are recycled oldest-first, with a bumped generation, so a stale
     * Entity handle from before a Destroy() call correctly reports itself as invalid via
     * IsValid(), even after its index has been reused by a new entity.
     *
     * @par Example
     * @code
     * EntityManager manager;
     * Entity player = manager.Create();
     * assert(manager.IsValid(player));
     *
     * manager.Destroy(player);
     * assert(!manager.IsValid(player)); // stale handle, now invalid
     * @endcode
     */
    class EntityManager {
    private:

        std::vector<uint32_t> _generations; ///< holds the current generation for each index
        std::queue<uint32_t>  _freeIndices; ///< holds the freed indices (held in a queue so that the oldest freed index gets reused first)

    public:
        
        // default constructor
        EntityManager() = default; 

        /**
         * @brief Creates a new Entity, reusing a freed index when one is available.
         * @return A newly valid Entity handle.
         * @complexity O(1)
         * @warning Not thread-safe.
         */
        Entity create();

        /**
         * @brief Destroys an Entity, freeing its index for future reuse.
         *
         * Bumps the destroyed index's generation, immediately invalidating every
         * outstanding copy of this Entity handle.
         *
         * @param entity The Entity to destroy.
         * @note Destroying an already-invalid Entity is a safe no-op, not an error.
         * @complexity O(1)
         */
        void destroy(Entity); 

        /**
         * @brief Checks whether an Entity handle is still valid.
         *
         * An Entity is valid if its index is currently allocated AND its generation
         * matches the generation currently stored for that index.
         *
         * @param entity The Entity to check. Safe to call with any value, including NULL_ENTITY.
         * @return true if the entity is currently alive.
         * @complexity O(1)
         */
        bool is_valid(Entity) const; 

        /**
         * @brief Returns the number of currently-alive entities.
         * @complexity O(1)
         */
        uint32_t get_alive_count() const { return static_cast<uint32_t>(_generations.size() - _freeIndices.size()); }; 

    }; 
}