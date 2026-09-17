/**
 * @file SparseSet.hpp
 * @author Shane David
 * @brief Defines the concrete SparseSet templated class 
 */
#pragma once 

#include <vector>  // to create _sparse, _dense, and _entitites
#include <cstdint> // for uint32_t 

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/SparseSetBase.hpp"

namespace Imago::ECS 
{
    /**
     * @brief SparseSet concrete class templated to a component type, acts as Imago ECS's component pool 
     * 
     * Packed, cache friendly storage for one component type. SparseSet<T> stores every entity's T component contiguously in memory, with no gaps.
     * This allows iterating every instance of T to be a straight linear scan with no pointer chasing and no skipped/empty slots.
     * Lookups, insertions, and removals are all O(1), implemented via three parallel arrays:
     * 
     * - '_sparse' maps an entitiy's raw index to its slot in '_dense'/'_entities'
     * - '_dense' is the packed component data itself
     * - '_entities' is the packed entities, parallel to '_dense', used to patch '_sparse' after a swap-and-pop removal
     * 
     * @tparam T the component type stored in the pool. 
     */
    template <typename T>
    class SparseSet : SparseSetBase {
    private:
        
        std::vector<uint32_t> _sparse;    ///< sparse array of Entities that maps an Entity's index to its slot in the _dense/_entity packed arrays
        std::vector<T>        _dense;     ///< dense array of components, packed so there are no gaps
        std::vector<Entity>   _entitites; ///< packed array of Entities, parallel to _dense

        static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFFu; ///< constant variable to represent a Null Entity/Invalid Index
    
    public:
        
        // default constructor
        SparseSet() = default; 

        /**
         * @brief adds a component for an Entity, growing the sparse array if necessary
         * @param e the Entity to place the component on
         * @param component the component to insert
         */
        void Insert(Entity e, T component); 

        /**
         * NOTE: a oonst and a non const getter is an important C++ mechanism called overlaoding on const
         * the compiler will pick which one to call based on whether the object calling it is itself const or not 
         * this is important for const correctness so that any const object can still get the entities in the pool
         */

        /**
         * @brief returns a reference to the Entity's component
         * @param e the Entity whose component you want
         * @return the referance to the Entity's component  
         */
        T& Get(Entity e);
        const T& Get(Entity e) const; 

        // SpareSetBase overrides
        void Remove(Entity e) override;
        void Has(Entity e) override;
        void GetSize() const override; 
        void std::vector<Entity>& GetEntities() const override; 
    }; 
}