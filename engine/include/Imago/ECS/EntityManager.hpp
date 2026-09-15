# pragma once 

#include <cstdint> // for uint32_t
#include <vector>  // for holding the Entity generations
#include <queue>   // for holding free indices

#include "Imago/ECS/Entity.hpp" // for Entity alias and bitwise math

// Owns the lifetime of every Entity in the engine, it creates, destroys, and valildates every entity
// Handles recyclying freed indices by bumping their generation to invalidate stale Entity handles. 
// Scoped to Imago::ECS
namespace Imago::ECS 
{

    class EntityManager {
    private:

        std::vector<uint32_t> _generations; // holds the current generation for each index
        std::queue<uint32_t>  _freeIndices; // holds the freed indices (held in a queue so that the oldest freed index gets reused first)

    public:
        
        // default constructor
        EntityManager() = default; 

        // creates a new Entity, reusing a freed index when necessary 
        Entity Create();

        // destroys an Entity, frees its index for reuse
        void Destroy(Entity entity); 

        // checks whether an Entity handle is still valid (has the current generation for its index)
        bool IsValid(Entity entity) const; 

        // returns the number of currently-alive entities
        uint32_t GetAliveCount() const { return static_cast<uint32_t>(_generations.size() - _freeIndices.size()); }; 

    }; 

}