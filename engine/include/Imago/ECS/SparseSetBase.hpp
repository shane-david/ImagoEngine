/**
 * @file SparseSetBase.hpp
 * @author Shane David
 * @brief Declares the SparseSetBase interface
 */
#pragma once

#include <vector> // for the dense, sparse, and entity arrays

#include "Imago/ECS/Entity.hpp"

namespace Imago::ECS 
{

    /**
     * @brief Type-erased interface for SparseSet<T>.
     * 
     * Lets Nexus store pools of different component types behind a single pointer type without knowing what T any given pool actually holds.
     * SparseSet<T> is the only intended implementation of this interface. 
     */
    class SparseSetBase {
    public:
        
        // virtual destructor
        virtual ~SparseSetBase() = default;
        
        /**
         * @brief removes an Entity's component from this pool, if it is currently present
         * @param e the Entity to remove
         */
        virtual void remove(Entity e) = 0; 

        /**
         * @brief checks whether an Entity currently has a component stored in this pool 
         * @param e the Entity to check 
         * @return true if the Entity has a component stored in this pool 
         * @return false if the Entity does not have a component stored in this pool 
         */
        virtual bool has(Entity e) const = 0;

        /**
         * @brief return the number of Entities currently stored in this pool 
         * @return the number of Entitities currently stored in the pool  
         */
        virtual size_t get_size() const = 0; 

        /**
         * @brief returns the dense array of entities currently stored in this pool 
         * @return the dense array of Entities 
         */
        virtual const std::vector<Entity>& get_entities() const = 0; 
    }; 
}