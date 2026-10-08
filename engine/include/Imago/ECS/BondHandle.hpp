/**
 * @file BondHandle.hpp
 * @author Shane David
 * @brief Defins the BondHandle, this is the view that users will access to iterate over the Entities of a Bond within a system. 
 */

#pragma once 

#include <cstddef> // for size_t 
#include <vector> // for storing the Entities to iterate over 
#include <cassert> // for asserts in constructor 

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/Bond.hpp"

namespace Imago::ECS 
{

    // Note: BondHandle<Components...> can take any number of component types. This is using a C++ variadic template parameter pack

    /**
     * @brief Lightweight view over a Bond. This is returned by value from Nexus::Bond<Components...>() and does not deal
     * with the structural changes relevant for bonds but just provieds an iterorator like SurveyHandle does.
     * 
     * Instead of checking every component for memebership in each component pool it just directly iterates over the Bond's sorted region. 
     * 
     * @tparam Components  The component types of the Bond. 
     */
    template <typename... Components> 
    class BondHandle {
    private:
        
        const Bond* _bond; ///< The Nexus-owned Bond to create a view for the user (non-owning) 

    public:

        /**
         * @brief Constructs a new BondHandle object, initializing it with is according Bond.
         * This can only be called by the Nexus but does not need the friend class structure because Bonds can only be created and owned by the Nexus. 
         * @param bond The Bond to view and iterate over. 
         */
        explicit BondHandle(const Bond* bond)
            : _bond(bond)
            {
                // ensure an empty array was not passed in 
                static_assert(sizeof...(Components) > 0, "[BondHandle] Must be queried for at least one component type."); 

                // ensure the bond is not null
                assert(_bond != nullptr && "[BondHandle] cannot be created with a nullptr Bond!");

                // ensure the bond's size and the query size are the same
                assert(_bond->get_pool_count() == sizeof...(Components) && "[BondHandle] BondHandle size and Bond pool count do not match!");
            }

        /**
         * @internal
         * @brief Iterator inner class for range-based for loops over the group.
         * It simply progressed the index until it reaches the bond size. 
         */
        class Iterator {
        private: 
            
            const std::vector<Entity>* _entities; ///< entity array of the Bond's first pool
            size_t _index; ///< current index into the bond 

        public:
                
            /**
             * @brief Construct a new Iterator object. 
             * @param entities The entity array to read from. 
             * @param index wWhere this specific Iterator points to at construction 
             */
            Iterator(const std::vector<Entity>* entities, size_t index)
                : _entities(entities), _index(index) { }

            /**
             * @brief Dereference the Iterator to access the current element. 
             * @return Entity that is at the current index.
             */
            Entity operator*() const 
            {
                return (*_entities)[_index]; 
            }

            /**
             * @brief Advances the Iterator to the next element which is just _index + 1 
             * @return Reference to the Iterator after going to the next element. 
             */
            Iterator& operator++() 
            {
                _index++; // increment index
                return *this; // return the iterator. 
            }

            /**
             * @brief Compare the indices of the two Iterators 
             * @param other Iterator to compare with 
             * @return true if the indices are not the same. 
             * @return false if the indices are the same. 
             */
            bool operator!=(const Iterator& other) const 
            {
                return _index != other._index; 
            }
        }; 

        /**
         * @brief Returns an Iterator to the first element.
         */
        Iterator begin() const 
        {
            return Iterator(&(_bond->get_entities()), 0);
        }

        /**
         * @brief Returns an Iterator past the last element. 
         */
        Iterator end() const 
        {
            return Iterator(&(_bond->get_entities()), _bond->get_bond_size()); 
        }
    }; 
}