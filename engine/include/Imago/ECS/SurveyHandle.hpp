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
     * @brief The Survey takes Componets as templated arguments and acts as an iterator over all entities with all component types
     * 
     * Survey defines an Iterator inner class so that iterating with for loops is safe. As it iterates it skips over any
     * index that represents an Entity that is not in every single component pool. 
     * 
     * @tparam Components to survey 
     */
    template <typename... Components>
    class SurveyHandle {
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
        explicit SurveyHandle(const std::array<SparseSetBase*, sizeof...(Components)>& pools) 
            : _pools(pools)
        {   
            // ensure an empty array was not passed in 
            static_assert(sizeof...(Components) > 0, "[Survey] Must be queried for at least one component type."); 

            // find and set the smallest pool with a linear scan 
            _smallestPool = _pools[0]; 
            for (SparseSetBase* pool : _pools) {
                if (pool->get_size() < _smallestPool->get_size()) {
                    _smallestPool = pool; 
                }
            }
        }

        // Note: since the most common use case for a Survey is iterating through it with a range-based forloop 
        // we need to define an Iterator inner class with the necessary methods

        /**
         * @internal
         * @brief Iterator inner class the defines operators for progressing through the Survey 
         * and the necessary overloads for functionality with for loops
         */
        class Iterator {
        private:
            
            // helper methods

            /**
             * @brief Advances _index past any entity that is not in every requested pool.
             */
            void skip_unmatched() 
            {
                // increment index while we are not at the end of the pools and every Entity is in each component pool
                while (_index < _entities->size() && !matches_all((*_entities)[_index])) {
                    _index++; 
                }
            }

            /**
             * @brief Determines whether or not a specified Entitys is in every requested pool.
             * @param e The Entity to check. 
             * @return true if the Entity is in every requested pool.
             * @return false if the Entity is not in every requested pool. 
             */
            bool matches_all(Entity e) const 
            {
                // iterate through every pool for the survey
                for (SparseSetBase* pool : *_pools) {

                    // if any entity does not have the pool return false
                    if (!pool->has(e)) {
                        return false; 
                    }
                }

                // if we make it out every entity had the pool so return true
                return true; 
            }


            // data 
            const std::vector<Entity>* _entities; ///< vector of Entities of the smallest pool
            size_t _index; ///< current index 
            const std::array<SparseSetBase*, sizeof...(Components)>* _pools; ///< pointer to the Survey's pools

        public:

            /**
             * @brief Construct a new Iterator object
             * @param entities the smallest pool of entitites in the Survey
             * @param index where this specific Iterator points to at construction 
             * @param pools the pools in the Survey 
             */
            Iterator(const std::vector<Entity>* entities, size_t index, const std::array<SparseSetBase*, sizeof...(Components)>* pools)
                : _entities(entities), _index(index), _pools(pools)
            {
                skip_unmatched(); 
            }

            // operator overloads for for loops

            /**
             * @brief Dereference the iterator to access the current element.
             * @return The Entity that is at the current index 
             */
            Entity operator*() const 
            {
                return (*_entities)[_index]; // dereferences should return the entity at the current index 
            }

            /**
             * @brief Advances the Iterator to the next element, skipping indices that represent Entities not in every pool.  
             * @return Reference to the Iterator after going to the next element. 
             */
            Iterator& operator++() 
            {
                _index++; // increment the index
                skip_unmatched(); // skip to the next valid index
                return *this; // return the iterator 
            }

            /**
             * @brief Checks whether two iterators point to different positions 
             * @param other Iterator to compare against. 
             * @return true if the Iterators refer to different positions. 
             * @return false if the Iterators refer to the same position. 
             */
            bool operator!=(const Iterator& other) const 
            {
                return _index != other._index; // comparisons should compare indices 
            }
            
        }; 

        // begin and end methods for for loop usage with Iterator, they just return two iterators at where the start should be and where the end should be 

        /**
         * @brief Returns an Iterator to the first element. 
         * 
         * @return Iterator pointing to the first element
         */
        Iterator begin() const 
        {
            return Iterator(&_smallestPool->get_entities(), 0, &_pools); 
        }   

        /**
         * @brief Returns an Iterator past the last element
         * 
         * @return Iterator representing the end of the collection
         */
        Iterator end() const 
        {
            return Iterator(&_smallestPool->get_entities(), _smallestPool->get_size(), &_pools); 
        }
    }; 
}