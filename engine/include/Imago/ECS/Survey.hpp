/**
 * @file Survey.hpp
 * @author Shane David
 * @brief Defines the Survey, how the users will iterate over Entities with specified components in Systems. 
 */

#pragma once

#include <array> // for array to store pools
#include <vector> // for entity vector in iterator

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/SparseSetBase.hpp"

// Note: Survey and Bonds will contain in class method definitions opposed to the style and structure of most ImagoECS classes, this is to avoid confusing 
// and noise with out of class definitions for a nested class 

namespace Imago::ECS 
{

    // Note: Survey<Components...> can take any number of component types. This is using a C++ variadic template parameter pack

    /**
     * @brief 
     * 
     * @tparam Components 
     */
    template <typename... Components>
    class Survey {
    private:
        
        // Note: we are using an array here instead of a vector because the count of components and hence the size of the _pools array will be known at compile time

        std::array<SparseSetBase*, sizeof...(Components)> _pools; ///> _pools the Survey is finding Entities for
        SparseSetBase* _smallestPool; ///> the smallest of the pools being surveyed 

    public: 

        // Note: The explicit keyword here prevents the compiler from converting std::array<SparseSetBase*, N> to a Survey 
        // which C++ allows for under normal circumstances since that is the only element being passed into the constructor


        /**
         * @brief Constructs a new Survey object by defining _pools and iterating through them to find the smallest pool
         * 
         * Will only be called by the Nexus where the nexus will send pointers to the pools that it owns. 
         * 
         * @param pools the array of SparseSetBase* that makes up the pools for the particular Survey. 
         */
        explicit Survey(const std::array<SparseSetBase*, sizeof...(Components)>& pools) 
            : _pools(pools)
        {   
            // ensure an empty array was not passed in 
            static_assert(sizeof...(Components) > 0, "[Survey] Must be queried for at least one component type."); 

            // find and set the smallest pool with a linear scan 
            _smallestPool = _pools[0]; 
            for (SparseSetBase* pool : _pools) {
                if (pool->GetSize() < _smallestPool->GetSize()) {
                    _smallestPool = pool; 
                }
            }
        }

        // Note: since the most common use case for a Survey is iterating through it with a range-based forloop 
        // we need to define an Iterator inner class with the necessary methods

        /**
         * @brief 
         * 
         */
        class Iterator {
        private:
            
            // helper methods

            /**
             * @brief Advances _index past any entity that is not in every requested pool.
             */
            void SkipUnmatched() 
            {
                // increment index while we are not at the end of the pools and every Entity is in each component pool
                while (_index < _entities->size() && !MatchesAll((*_entities)[_index])) {
                    _index++; 
                }
            }

            /**
             * @brief Determines whether or not a specified Entitys is in every requested pool.
             * @param e The Entity to check. 
             * @return true if the Entity is in every requested pool.
             * @return false if the Entity is not in every requested pool. 
             */
            bool MatchesAll(Entity e) const 
            {
                // iterate through every pool for the survey
                for (SparseSetBase* pool : *_pools) {

                    // if any entity does not have the pool return false
                    if (!pool->Has(e)) {
                        return false; 
                    }
                }

                // if we make it out every entity had the pool so return true
                return true; 
            }


            // data 
            const std::vector<Entity>* _entities; ///< vector of Entities that have all components in the Survey 
            size_t _index; ///< current index 
            const std::array<SparseSetBase*, sizeof...(Components)>* _pools; ///< pointer to the Survey's pools

        public:

            Iterator(const std::vector<Entity>* entities, size_t index, const std::array<SparseSetBase*, sizeof...(Components)>* pools)
                : _entities(entities), _index(index), _pools(pools)
            {
                SkipUnmatched(); 
            }

            // opeartor overloads for for loops

            Entity operator*() const 
            {
                return (*_entities)[_index]; // dereferences should return the entity at the current index 
            }

            Iterator& operator++() 
            {
                _index++; // increment the index
                SkipUnmatched(); // skip to the next valid index
                return *this; // return the iterator 
            }

            bool operator!=(const Iterator& other) const 
            {
                return _index != other._index; // comparisons should compare indices 
            }
            
        }; 

        // begin and end methods for for loop usage with Iterator, they just return two iterators at where the start should be and where the end should be 
        Iterator begin() const 
        {
            return Iterator(&_smallestPool->GetEntities(), 0, &_pools); 
        }

        Iterator end() const 
        {
            return Iterator(&_smallestPool->GetEntities(), _smallestPool->GetSize(), &_pools); 
        }
    }; 
}