#include "Imago/ECS/Nexus.hpp"

namespace Imago::ECS 
{

    Nexus::Nexus() 
        : _commandBuffer(*this) {}

    Entity Nexus::create() 
    {
        return _entityManager.create(); 
    }

    //TODO: set up error messaging so it reports it through Nexus to avoid user confusion 
    void Nexus::destroy(Entity e) 
    {
        _commandBuffer.defer_destroy(e); 
    }

    void Nexus::destroy_immediate(Entity e) 
    {
        // iterate through pools 
        for (const auto& [_, value] : _pools) {

            // check if that pool contains the entity and remove the entity 
            //TODO: once logging is figured out remove the has check because remove() already does it 
            if (value->has(e)) {
                value->remove(e); 
            }
        }

        // destroy the Entity in the entity manager
        _entityManager.destroy(e); 
    }

    bool Nexus::is_valid(Entity e) const 
    {
        return _entityManager.is_valid(e); 
    }

    void Nexus::flush() 
    {
        _commandBuffer.flush(); 
    }
}