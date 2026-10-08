#include "Imago/ECS/Bond.hpp"

#include <utility> 

namespace Imago::ECS 
{

    Bond::Bond(std::vector<SparseSetBase*> pools)
        : _ownedPools(std::move(pools)) 
        {
            // ensure an empty array was not passed in 
            assert(!_ownedPools.empty() && "[Bond] Must own at least one component pool."); 

            // get the first pool of entities
            const std::vector<Entity>& entities = _ownedPools[0]->get_entities(); 

            // sort all entities with all components to front 
            for (Entity e : entities) {
                if (matches_all(e)) {
                    move_entity(e, _bondSize++); 
                }
            }
        }

    bool Bond::matches_all(Entity e) const 
    {
        // iterate through every pool the bond owns 
        for (SparseSetBase* pool : _ownedPools) {

            // if any pool does not have the entity it does not match all
            if (!pool->has(e)) {
                return false; 
            }
        }

        // if we make it out of the loop, every pool has the entity
        return true; 
    }

    void Bond::move_entity(Entity e, size_t pos) 
    {
        // iterate through every pool the bond owns 
        for (SparseSetBase* pool : _ownedPools) {

            // get the entity currently at pos
            Entity other = pool->get_entities().at(pos); 

            // swap for the current pool 
            pool->swap_entities(e, other); 
        }
    } 

    void Bond::on_component_bound(Entity e) 
    {
        // return if the entity still is not in every component pool
        if (!matches_all(e)) return; 

        // return if the entity is already considered part of the bond
        if (_ownedPools[0]->get_index(e) < _bondSize) return; 

        // otherwise swap it into the group 
        move_entity(e, _bondSize++); 
    }

    void Bond::on_component_unbound(Entity e) 
    {
        // return if the entity still is not in every component pool
        if (!matches_all(e)) return; 

        // return if the entity is already considered not part of the bond
        if (_ownedPools[0]->get_index(e) >= _bondSize) return; 

        // otherwise swap it out of the group
        move_entity(e, --_bondSize); 
    }

    size_t Bond::get_bond_size() const {
        return _bondSize; 
    }

    const std::vector<Entity>& Bond::get_entities() const {
        return _ownedPools[0]->get_entities(); 
    }

    size_t Bond::get_pool_count() const {
        return _ownedPools.size(); 
    }

}