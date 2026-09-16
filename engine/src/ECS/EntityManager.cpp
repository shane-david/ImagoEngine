#include "Imago/ECS/EntityManager.hpp"

#include <cassert> // for error checking with entity index and generation bounds
#include <spdlog/spdlog.h> // for error logging 

// scoped to Imago::ECS
namespace Imago::ECS 
{

    Entity EntityManager::Create() 
    {

        // TODO: create non-crash solution for if too many Entity's are created
        assert(GetAliveCount() < ENTITY_INDEX_MASK && "[ENTITYMANAGER] Max Entities have been generated");

        // recycle and index if there is a free one 
        if (_freeIndices.size() > 0) {

            // get the last freed index and remove it
            uint32_t lastFreed = _freeIndices.front(); 
            _freeIndices.pop(); 

            // get the index's current generation (already incremented by destroy so we do not need to do it here)
            uint32_t lastGeneration = _generations[lastFreed]; 

            // pack the recycled index and its generation into an Entity
            return MakeEntity(lastFreed, lastGeneration); 

        // if there is not a free index just create a new one 
        } else {

            // next index is always the size of _generations
            uint32_t nextIndex = static_cast<uint32_t>(_generations.size()); 

            // add a new element to generations with generation 0 (already going to be at nextIndex)
            _generations.push_back(0); 

            // pack the new index and its generation 0 together into an Entity
            return MakeEntity(nextIndex, 0); 

        }
    }

    void EntityManager::Destroy(Entity e) 
    {

        // get the validity of the Entity
        bool valid = IsValid(e); 

        // for release, if an entity is invalid just dont worry about destroying it 
        if (!valid) {
            spdlog::debug("[ENTITYMANAGER] Destroy called on an already-invalid Entity (index {}, generation{})", GetEntityIndex(e), GetEntityGeneration(e)); 
            return; 
        }

        // extract the index from the Entity
        uint32_t index = GetEntityIndex(e); 

        // make sure that there is a next valid generation and bump it if so
        //TODO: create non-crash solution for if the max generation is reached 
        assert(_generations.at(index) < ENTITY_GENERATION_MASK && "[ENTITYMANAGER] The Entity you are trying to delete has hit its generation limit");
        _generations.at(index)++; 

        // free the index for reuse 
        _freeIndices.push(index); 
    }

    bool EntityManager::IsValid(Entity e) const 
    {
        
        // extract the index and generation from the Entity
        uint32_t index = GetEntityIndex(e); 
        uint32_t generation = GetEntityGeneration(e); 

        // make sure the index is in the bounds of _generations and that the Entity's generation matches the one in _generations
        if (index < static_cast<uint32_t>(_generations.size()) && _generations[index] == generation) {
            return true; 
        }

        // if it did not make it to the return true statement the Entity is invalid
        return false; 
    }
}