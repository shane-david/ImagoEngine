/**
 * @file Bond.hpp
 * @author Shane David
 * @brief Defines the Bond class which will be a permanent group of component pools that are kept sorted
 * so that the entities with every component in the Bond sit at the front 
 */

#pragma once

#include <vector> // to store the SparseSets
#include <cstddef> // for size_t

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/SparseSetBase.hpp"

namespace Imago::ECS 
{
    
    /**
     * @brief A Bond permanently reorders every pool within it so that every Entity that has all of the components in the Bond sits at the fround of every pool, in the same index at each.
     * Bonds should be used for iterative systems that will execute every frame in every situation (Rendering, Physics, ect.) becuase the time save from no longer checking whether or not 
     * each Entity has every component starts to become significant for these systems. 
     * 
     * However, each Component Type can only be bonded once as whichever Bond it is a part of now has full ownership of that Component pool, so it is important to choose wisely what 
     * Bonds you are creating.
     * 
     * The Bond is owned and notified by the Nexus so that it can be resorted whenever component data changes. 
     */
    class Bond {
    private: 
        
        std::vector<SparseSetBase*> _ownedPools; ///< pools this Bond has exclusive ownership of (right to reorder)
        size_t _bondSize = 0; ///< number of Entities at the front of every pool that have all Bonded components 

        friend class Nexus; 
        
        /**
         * @brief Constructs a new Bond object with the given pools and sorts the Entities with every component type to the front of each pool. 
         * @param pools The pools to own. Must not be empty. 
         */
        explicit Bond(std::vector<SparseSetBase*> pools); 

        /**
         * @brief Determines whether or not a specified Entity is in every requested pool.
         * @param e The Entity to check. 
         * @return true if the Entity is in every pool.
         * @return false if the Entity is not in every pool. 
         */
        bool matches_all(Entity e) const; 

        /**
         * @brief Swaos e with whichever Entity is currently at the requestd position. 
         * @param e The Entity to move. It should be owned in every pool if it is being swapped to the front. 
         * @param pos The position to move the Entity to. 
         */
        void move_entity(Entity e, size_t pos); 

        /**
         * @brief Called by the Nexus after a component is bound to one of the bonded pools so that it can move 
         * e to the front elements if it now has every bonded component. 
         * @param e The Entity that a component was just bound to. 
         */
        void on_component_bound(Entity e); 

        /**
         * @brief Called by the Nexus before a component is unbound from one of the bonded pools that that it can move
         * e out of the front elements since it no longer has every bonded component. It moves it before the component 
         * is officially unbound so that SparseSet's swap-and-pop only every touches the Entities that are not at the front.
         * This is safe to call more than once for the same Entity. 
         * @param e The Entity that a component is about to be unbound from. 
         */
        void on_component_unbound(Entity e); 

    public:

        // explicitly disallow copy behavior
        Bond(const Bond&) = delete; 
        Bond& operator=(const Bond&) = delete; 

        /**
         * @brief Returns the number of Entities that are in the bond (have every component) 
         */
        size_t get_bond_size() const; 

        /**
         * @brief Returns the entity array of the first owned pool. 
         */
        const std::vector<Entity>& get_entities() const; 

        /**
         * @brief Returns the number of component pools in the bond 
         */
        size_t get_pool_count() const; 
    }; 
}