/**
 * @file SparseSet.hpp
 * @author Shane David
 * @brief Defines the concrete SparseSet templated class 
 */
#pragma once 

#include <vector>  // to create _sparse, _dense, and _entitites
#include <cstdint> // for uint32_t 
#include <spdlog/spdlog.h> // for error logging 
#include <utility> // for std::move
#include <cassert> 

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
     * - `_sparse` maps an entitiy's raw index to its slot in `_dense`/`_entities`
     * - `_dense` is the packed component data itself
     * - `_entities` is the packed entities, parallel to `_dense`, used to patch `_sparse` after a swap-and-pop removal
     * 
     * @tparam T the component type stored in the pool. 
     */
    template <typename T>
    class SparseSet : public SparseSetBase {
    private:
        
        std::vector<uint32_t> _sparse;    ///< sparse array of Entities that maps an Entity's index to its slot in the _dense/_entity packed arrays
        std::vector<T>        _dense;     ///< dense array of components, packed so there are no gaps
        std::vector<Entity>   _entities; ///< packed array of Entities, parallel to _dense

        static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFFu; ///< constant variable to represent a Null Entity/Invalid Index
    
    public:
        
        // default constructor
        SparseSet() = default; 

        /**
         * @brief adds a component for an Entity, growing the sparse array if necessary
         * @param e the Entity to place the component on
         * @param component the component to insert
         * @return a reference to the component
         */
        T& insert(Entity e, T component); 

        /**
         * @brief replaces a component with new data 
         * @param e the Entity whose component needs to be replaced
         * @param component the component to replace it with
         * @return a reference to the component 
         */
        T& replace(Entity e, T component); 

        
         // NOTE: a const and a non const getter is an important C++ mechanism called overlaoding on const
         // the compiler will pick which one to call based on whether the object calling it is itself const or not 
         // this is important for const correctness so that any const object can still get the entities in the pool

        /**
         * @brief returns a reference to the Entity's component
         * @param e the Entity whose component you want
         * @return the referance to the Entity's component  
         */
        T& get(Entity e);
        const T& get(Entity e) const; 

        /**
         * @brief tries to get a component and returns nullptr if one does not exist
         * @param e the entity to try to get
         * @return component pointer or nullptr if no component exists 
         */
        T* try_to_get(Entity e); 

        // SpareSetBase overrides
        void remove(Entity e) override;
        bool has(Entity e) const override;
        size_t get_size() const override; 
        const std::vector<Entity>& get_entities() const override; 
    }; 

    //------------
    //definitions
    //------------

    template <typename T>
    T& SparseSet<T>::insert(Entity e, T component) 
    {

        // extract the Entity's index
        uint32_t index = get_entity_index(e); 

        // ensure that the Entity's size is under the maximum entities
        assert(index < ENTITY_INDEX_MASK && "[SparseSet] Insert call with an out-of-range Entity index"); 

        // ensure that the Entity does not already have a component
        if (index < _sparse.size() && _sparse[index] != INVALID_INDEX) {
            spdlog::warn("[SparseSet] You are trying to add a component that Entity {} already has. Please use Nexus.Patch() instead", index); 
            return _dense[_sparse[index]]; 
        }

        // if the Entity's index is bigger than sparse, resize it to that index
        if (index >= _sparse.size()) {
            _sparse.resize(index + 1, INVALID_INDEX); 
        }

        // the current size if _dense/_entities is the next index so add that number to _sparse at the Entity's index
        _sparse[index] = static_cast<uint32_t>(_dense.size()); 

        // add the component to _dense and entity to _entities
        _dense.push_back(std::move(component));
        _entities.push_back(e); 

        // return the component for future additions
        return _dense.back(); 

    }

    template <typename T>
    T& SparseSet<T>::replace(Entity e, T component) 
    {
        // extract the Entity's index 
        uint32_t index = get_entity_index(e); 

        // ensure the entity has a component to replace
        assert(index < _sparse.size() && _sparse[index] != INVALID_INDEX && "[SparseSet] Replace called on an Entity with no existing component");

        // get the component index 
        uint32_t componentIndex = _sparse[index]; 

        // use that index to move the new component into its olds slot
        _dense[componentIndex] = std::move(component); 

        return _dense[componentIndex]; 
        
    }

    template <typename T>
    T& SparseSet<T>::get(Entity e) {

        // extract the index 
        uint32_t index = get_entity_index(e); 

        // ensure the index is in bounds
        assert(index < _sparse.size() && _sparse[index] != INVALID_INDEX && "[SparseSet] Get called on an Entity with no component in this pool"); 

        // return the componen
        return _dense[_sparse[index]]; 
    }

    template <typename T>
    const T& SparseSet<T>::get(Entity e) const {

        // extract the index 
        uint32_t index = get_entity_index(e); 

        // ensure the index is in bounds
        assert(index < _sparse.size() && _sparse[index] != INVALID_INDEX && "[SparseSet] Get called on an Entity with no component in this pool"); 

        // return the component
        return _dense[_sparse[index]]; 
    }

    template <typename T>
    T* SparseSet<T>::try_to_get(Entity e) {

        // extract the index
        uint32_t index = get_entity_index(e);

        // return nullptr if the index is not in bounds or does not exist
        if (index >= _sparse.size() || _sparse[index] == INVALID_INDEX) {
            return nullptr; 
        }

        //othewrise return the component
        return &_dense[_sparse[index]]; 
    }
    

    template <typename T>
    void SparseSet<T>::remove(Entity e) {

        // unpack the Entity index
        uint32_t index = get_entity_index(e); 

        // ensure that the entity actually does have a component in the sparse set
        if (index >= _sparse.size() || _sparse[index] == INVALID_INDEX) {
            spdlog::warn("[SparseSet] Cannot remove component at index {} because there is no component to remove.", index);
            return; 
        }

        // get the index to be removed
        uint32_t removedIndex = _sparse[index]; 

        // get the last index 
        uint32_t lastIndex = static_cast<uint32_t>(_dense.size() - 1); 

        // if the index is not the last index we to swap it to the last index
        if (removedIndex != lastIndex) {

            // overwrite the remove component with the last component for _dense and _entities
            _dense[removedIndex] = _dense[lastIndex];
            _entities[removedIndex] = _entities[lastIndex]; 

            // get the index of the entity we just swapped
            uint32_t swappedIndex = get_entity_index(_entities[removedIndex]); 

            // update _sparse to now contain the new index for the entity we just swapped
            _sparse[swappedIndex] = removedIndex; 
        }

        // remove the last element from both packed arrays
        _dense.pop_back();
        _entities.pop_back(); 

        // reset the removed entity's index to invalid
        _sparse[index] = INVALID_INDEX; 
    }

    template <typename T>
    bool SparseSet<T>::has(Entity e) const {

        // extract index and return if it is in the sparse array
        uint32_t index = get_entity_index(e);
        return index < _sparse.size() && _sparse[index] != INVALID_INDEX; 
    }

    template <typename T>
    size_t SparseSet<T>::get_size() const {
        return _dense.size(); 
    }

    template <typename T>
    const std::vector<Entity>& SparseSet<T>::get_entities() const {
        return _entities; 
    }
}