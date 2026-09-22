#include "Imago/ECS/Nexus.hpp"

namespace Imago::ECS 
{

    Entity Nexus::Create() 
    {
        return _entityManager.Create(); 
    }

    //TODO: set up error messaging so it reports it through Nexus to avoid user confusion 
    void Nexus::Destroy(Entity e) 
    {

        // iterate through pools 
        for (const auto& [_, value] : _pools) {

            // check if that pool contains the entity and remove the entity 
            //TODO: once logging is figured out remove the has check because Remove() already does it 
            if (value->Has(e)) {
                value->Remove(e); 
            }
        }

        // destroy the Entity in the entity manager
        _entityManager.Destroy(e); 
    }

    bool Nexus::IsValid(Entity e) const 
    {
        return _entityManager.IsValid(e); 
    }
}