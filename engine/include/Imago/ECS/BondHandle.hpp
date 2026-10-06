/**
 * @file BondHandle.hpp
 * @author Shane David
 * @brief Defins the BondHandle, this is the view that users will access to iterate over the Entities of a Bond within a system. 
 */

#pragma once 

#include <cstddef> // for size_t 
#include <vector> // for storing the Entities to iterate over 

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
        explicit BondHandle(const Bond* bond); 

        /**
         * @internal
         * @brief Iterator inner class for range-based for loops over the group.
         * It simply progressed the index until it reaches the bond size. 
         */
        class Iterator {
        private: 
            
            const std::vector<Entity>* _entities; ///> entity array of the Bond's first pool
            size_t index; ///> current index into the bond 

        public:
                
            /**
             * @brief Construct a new Iterator object. 
             * @param entities The entity array to read from. 
             * @param index wWhere this specific Iterator points to at construction 
             */
            Iterator(const std::vector<Entity>* entities, size_t index); 

            Entity operator*() const; 
            Iterator& operator++(); 
            bool operator!=(const Iterator& other) const; 
        }

        /**
         * @brief Returns an Iterator to the first element.
         */
        Iterator begin() const;

        /**
         * @brief Returns an Iterator past the last element. 
         */
        Iterator end() const; 


    }; 
}