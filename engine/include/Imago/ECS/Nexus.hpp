/**
 * @file Nexus.hpp
 * @author Shane David 
 * @brief Defines the Nexus, the brain of the ECS world 
 */

#pragma once

#include <cstdint> // for uint32_t 
#include <unordered_map> // for component pools
#include <memory> // for std::unique_ptr

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/SparseSet.hpp"
#include "Imago/ECS/SparseSetBase.hpp"

namespace Imago::ECS 
{

    /// @brief alias for component ids for pool storage
    using ComponentTypeId = uint32_t; 

    // NOTE: Here we use the Meyer's Singleton:
    // the detail namespace contains a function for getting the next id, the first line only runs once because it is a static loca variable 
    // and hence the memory is only allocated once, the return statement returns the id and then increments it for every call so that we get a new, sequential id every call 
    // after it is a templated function that returns the id for each component type. Since it is templated a new version of this function is compiled for each type passed through
    // meaning that the static id variable is assigned only once for each component type and that specific id is returned whenever the function for that component type is called. 
    // This allows us to to match component types to their corresponding id dynamically and without the compiler having to mange one persisten id slot per type 

    /// @internal
    namespace detail 
    {
        /**
         * @brief Returns the next sequential ComponentTypeId, starting at 0. 
         * 
         * Backed by a single static counter shared across every call, regardless of caller. 
         * This should not be called directly. GetComponentTypeId<T>() should be used instead to ensure
         * that each component type only ever consumes one sequential id. 
         * 
         * @return the next id
         */
        inline ComponentTypeId NextComponentTypeId() 
        {
            static ComponentTypeId id = 0; // allocated only once for the first function call
            return id++; 
        }
    }

    /**
     * @brief returns the ComponenTypeId for component of type T.
     * 
     * The id is assigned the first time this is called for a given component of type T. Sice the id 
     * is static the id is the same for every following call with that component type.
     * 
     * @tparam The component type.
     * @return The id for that component type.
     */
    template <typename T>
    ComponentTypeId GetComponentTypeId() 
    {
        static ComponentTypeId id = detail::NextComponentTypeId(); // assigned only once for the first function call
        return id; 
    }

    /**
     * @brief 
     * 
     */
    class Nexus {
    private:
        
        std::unordered_map<ComponentTypeId, std::unique_ptr<SparseSetBase>> _pools; ///> unorder map of component pools, one SparseSetBase per component type, keyed with ids
        
        /**
         * @brief Returns the pool of the component type T. 
         * 
         * If the pool does not exist it creates the pool and returns the new one. 
         * 
         * @tparam T The component pool to get. 
         * @return Pointer to the component pool. 
         */
        template <typename T>
        SparseSet<T>* GetPool();

        /**
         * @brief Tries to return the pool of component type T.
         * @tparam T T The component pool to look for.
         * @return Pointer to the component pool if it exists. 
         * @return nullptr if the component pool does not exist. 
         */
        template <typename T>
        SparseSet<T>* FindPool(); 

    public:
        
        // default constructor
        Nexus() = default; 

        // explicitly disallow copy behavior
        Nexus(const Nexus&) = delete; 
        Nexus& operator=(const Nexus&) = delete; 

        /**
         * @brief Binds a component of type T to Entity e
         * 
         * Bind<T>(e, component) creates or fetches a pool for component type T depending on if it already exists or not. 
         * Binding a component to an Entity that already has that component will flag a warning and do nothing,
         * replacing component data must be done through Patch<T>()
         * 
         * @tparam T The component type to bind.
         * @param e The entity to bind the component to.
         * @param component The actual component data to bind
         * @return A reference to the component 
         */
        template <typename T>
        T& Bind(Entity e, T component); 

        /**
         * @brief Unbinds a component of type T from Entity e if possible. 
         * @tparam T The component type to unbind.
         * @param e The entity to unbind the component from. 
         */
        template <typename T>
        void Unbind(Entity e); 

        /**
         * @brief Overwrites an Entity's existing component of type T with new component data, 
         * 
         * The Entity must already have this component bound. See Bind<T>() for adding a brand new component. 
         * 
         * @tparam T The component type to patch.
         * @param e The Entity whose component to patch. 
         * @param component The actual new component data. 
         * @return A reference to the updated component. 
         */
        template <typename T>
        T& Patch(Entity e, T component); 

        /**
         * @brief Returns a reference to the Entity's component of type T. 
         *  
         * Get<T>(e) assumes e has that component type and should be used only 
         * in Surveys/Bonds where the entity is already guaranteed to have that component.
         * See, TryToGet<T>(e) for a safer version.
         * 
         * @tparam T The component type to return.
         * @param e The entity whose component to return.
         * @return Reference to the Entity's component. 
         */
        template <typename T>
        T& Get(Entity e); 

        /**
         * @brief Returns a reference to the Entity's component of type T if it has it and nullptr otherwise. 
         * @tparam T The component type to retur.
         * @param e The Entity whose component to return. 
         * @return Pointer to the component if the Entity has the component
         * @return nullptr if the Entity does not have the component.
         */
        template <typename T>
        T* TryToGet(Entity e); 

        /**
         * @brief Check whether an Entity currently has a component of type T.
         * @tparam T The component type to check for. 
         * @param e The Entity to check for the component. 
         * @return true if the Entity has the component. 
         * @return false if the Entity does not have the component. 
         */
        template <typename T>
        bool Has(Entity e); 
    }; 

    //-----------
    //definitions
    //-----------

    template <typename T>
    SparseSet<T>* Nexus::GetPool() 
    {

        // get the id at that component type
        uint32_t id = GetComponentTypeId<T>(); 

        // try to find the id in _pools
        auto location = _pools.find(id); 

        // if it does not exist, create a new one 
        if (location == _pools.end()) {
            location = _pools.emplace(id, std::make_unique<SparseSet<T>>()).first; 
        }

        // return the pool at that location casted to a raw SparseSet<T> pointer
        return static_cast<SparseSet<T>*>(location->second.get()); 
    }

    template <typename T>
    SparseSet<T>* Nexus::FindPool() 
    {

        // get the id at that component type
        uint32_t id = GetComponentTypeId<T>(); 

        // try to find it the id in _pools
        auto location = _pools.find(id);

        // if it does not exist return nullptr
        if (location == _pools.end()) {
            return nullptr;
        }

        // otherwise return the pool casted to a raw SparseSet<T> pointer
        return static_cast<SparseSet<T>*>(location->second.get()); 
    }


}
