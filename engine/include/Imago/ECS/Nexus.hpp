/**
 * @file Nexus.hpp
 * @author Shane David 
 * @brief Defines the Nexus, the brain of the ECS world 
 */

#pragma once

#include <cstdint> // for uint32_t 
#include <unordered_map> // for component pools
#include <array> // to construct surveys 
#include <memory> // for std::unique_ptr
#include <utility> // for std::move
#include <cassert> // for bond asserts
#include <cstdlib> // for std::abort
#include <vector> // for bonds 

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/EntityManager.hpp"
#include "Imago/ECS/SparseSet.hpp"
#include "Imago/ECS/SparseSetBase.hpp"
#include "Imago/ECS/SurveyHandle.hpp"
#include "Imago/ECS/CommandBuffer.hpp"
#include "Imago/ECS/Bond.hpp"
#include "Imago/ECS/BondHandle.hpp"

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
        inline ComponentTypeId next_component_type_id() 
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
    ComponentTypeId get_component_type_id() 
    {
        static const ComponentTypeId id = detail::next_component_type_id(); // assigned only once for the first function call
        return id; 
    }

    /**
     * @brief the Nexus is the brain of the ECS world. It is where the user will 
     * interact with everything ECS related in systems. 
     * 
     * It handles:
     * - creating/destorying entities
     * - binding/unbinding components
     * - returning and creating bonds and surveys 
     */
    class Nexus {
    private:
        
        EntityManager _entityManager; ///< Nexus owns entity lifetime and is the only intended caller of the EntityManager
        CommandBuffer _commandBuffer; ///< Nexus owns the lifetime of a single CommandBuffer for all deferred commands 
        std::unordered_map<ComponentTypeId, std::unique_ptr<SparseSetBase>> _pools; ///< unorder map of component pools, one SparseSetBase per component type, keyed with ids
        std::vector<std::unique_ptr<Bond>> _bonds; ///< vector of all created bonds as unique pointers because the Nexus owns their entire lifetime 
        std::unordered_map<ComponentTypeId, Bond*> _bondMap; ///< map from component types to bonds to tell us what bonds are associated with what component types, raw pointer because bonds are owned in _bonds
        
        /**
         * @brief Returns the pool of the component type T. 
         * 
         * If the pool does not exist it creates the pool and returns the new one. 
         * 
         * @tparam T The component pool to get. 
         * @return Pointer to the component pool. 
         */
        template <typename T>
        SparseSet<T>* get_pool();

        /**
         * @brief Tries to return the pool of component type T.
         * @tparam T T The component pool to look for.
         * @return Pointer to the component pool if it exists. 
         * @return nullptr if the component pool does not exist. 
         */
        template <typename T>
        SparseSet<T>* find_pool(); 

        /**
         * @brief performs an Entity's destruction immediately instead of routing through the command buffer
         * @param e The entity to destroy
         */
        void destroy_immediate(Entity e); 

        /**
         * @brief performs a bind on an Entity immediately instead of routing through the command buffer
         * @tparam The component type to be bound
         * @param e The entity to bind the component to
         * @param component The comonent data
         */
        template <typename T>
        void bind_immediate(Entity e, T component); 

        /**
         * @brief performs an unbind on an Entity immediately instead of routing through the command buffer
         * @tparam The component type to be unbound
         * @param e The entity whose coomponent of type T is being unbound 
         */
        template <typename T>
        void unbind_immediate(Entity e); 

        template <typename T>
        Bond* find_bond(); 

    public:

        // NOTE: the Nexus need to be a friend class with the CommandBuffer so that the CommandBuffer can access its private methods for making immediate changes
        friend class CommandBuffer; 

        /**
         * @brief Construct a new Nexus object and instantiates the command buffer
         */
        Nexus(); 

        // explicitly disallow copy behavior
        Nexus(const Nexus&) = delete; 
        Nexus& operator=(const Nexus&) = delete; 

        //-------------------------
        //Entity/Component Methods
        //-------------------------

        /**
         * @brief Creates a new Entity
         * @return The Entity packed uint32_t that was just created
         */
        Entity create(); 

        /**
         * @brief Destroys an Entity and removes it from every component pool it is in.
         * @param e The Entity to destroy
         */
        void destroy(Entity e); 

        /**
         * @brief Checks whether an Entity is still valid
         * 
         * This is a structual change so it is automatically routed to the command buffer and the change is actually made 
         * in destroy_immediate at the end of the current frame.
         * 
         * @param e The Entity to check
         * @return true if the Entity is valid
         * @return false if the Entity is not valid
         */
        bool is_valid(Entity e) const; 

        /**
         * @brief Binds a component of type T to Entity e
         * 
         * Bind<T>(e, component) creates or fetches a pool for component type T depending on if it already exists or not. 
         * Binding a component to an Entity that already has that component will flag a warning and do nothing,
         * replacing component data must be done through Patch<T>()
         * 
         * This is a structual change so it is automatically routed to the command buffer and the change is actually made 
         * in bind_immediate at the end of the current frame.
         * 
         * @tparam T The component type to bind.
         * @param e The entity to bind the component to.
         * @param component The actual component data to bind
         * @return A reference to the component 
         */
        template <typename T>
        void bind(Entity e, T component); 

        /**
         * @brief Unbinds a component of type T from Entity e if possible. 
         * 
         * This is a structual change so it is automatically routed to the command buffer and the change is actually made 
         * in unbind_immediate at the end of the current frame.
         * 
         * @tparam T The component type to unbind.
         * @param e The entity to unbind the component from. 
         */
        template <typename T>
        void unbind(Entity e); 

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
        void patch(Entity e, T component); 

        /**
         * @brief Returns a reference to the Entity's component of type T. 
         *  
         * Get<T>(e) assumes e has that component type and should be used only 
         * in Surveys/Bonds where the entity is already guaranteed to have that component.
         * See, try_to_get<T>(e) for a safer version.
         * 
         * @tparam T The component type to return.
         * @param e The entity whose component to return.
         * @return Reference to the Entity's component. 
         */
        template <typename T>
        T& get(Entity e); 

        /**
         * @brief Returns a reference to the Entity's component of type T if it has it and nullptr otherwise. 
         * @tparam T The component type to retur.
         * @param e The Entity whose component to return. 
         * @return Pointer to the component if the Entity has the component
         * @return nullptr if the Entity does not have the component.
         */
        template <typename T>
        T* try_to_get(Entity e); 

        /**
         * @brief Check whether an Entity currently has a component of type T.
         * @tparam T The component type to check for. 
         * @param e The Entity to check for the component. 
         * @return true if the Entity has the component. 
         * @return false if the Entity does not have the component. 
         */
        template <typename T>
        bool has(Entity e); 

        //-------------------
        //Survey/Bond methods
        //-------------------

        /**
         * @brief Returns a survey of the specified components
         * 
         * @tparam Components to survey
         * @return The SurveyHandle of the queried components 
         */
        template <typename... Components>
        SurveyHandle<Components...> survey(); 

        /**
         * @brief Returns a BondHandle (a view of the Bond) if one already exists for the components 
         * and creates one if possible and returns the view if one does not already exist. 
         * 
         * @tparam Components to Bond
         * @return The BondHandle of bonded components
         */
        template <typename... Components>
        BondHandle<Components...> bond(); 

        //-----------------------
        //Command Buffer Methods
        //-----------------------

        /**
         * @brief calls _commandBuffer.flush() to call the immediate methods that actually apply the deferred changes in the CommandBuffer
         */
        void flush(); 

    }; 

    //-----------
    //definitions
    //-----------

    template <typename T>
    SparseSet<T>* Nexus::get_pool() 
    {
        // get the id at that component type
        uint32_t id = get_component_type_id<T>(); 

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
    SparseSet<T>* Nexus::find_pool() 
    {
        // get the id at that component type
        uint32_t id = get_component_type_id<T>(); 

        // try to find it the id in _pools
        auto location = _pools.find(id);

        // if it does not exist return nullptr
        if (location == _pools.end()) {
            return nullptr;
        }

        // otherwise return the pool casted to a raw SparseSet<T> pointer
        return static_cast<SparseSet<T>*>(location->second.get()); 
    }

    template <typename T>
    void Nexus::bind_immediate(Entity e, T component) 
    {
        // get the pool for that component type
        SparseSet<T>* pool = get_pool<T>(); 

        // add the component to that pool
        pool->insert(e, std::move(component)); 

        // notify the bonds about the bind if any (this goes after the insert)
        Bond* currentBond = find_bond<T>(); 
        if (currentBond != nullptr) {
            currentBond->on_component_bound(e); 
        }
    }

    template <typename T>
    void Nexus::unbind_immediate(Entity e) 
    {
        // get the pool for that component type
        SparseSet<T>* pool = find_pool<T>();
        if (pool == nullptr) return; 

        // notify the bonds about the unbind if any (this goes before the removal)
        Bond* currentBond = find_bond<T>();
        if (currentBond != nullptr) {
            currentBond->on_component_unbound(e); 
        }

        // remove the component from that pool
        pool->remove(e); 
    }

    template <typename T>
    Bond* Nexus::find_bond() 
    {
        // get the id at the component type 
        ComponentTypeId id = get_component_type_id<T>(); 

        // try to find the id in _bondMap
        auto location = _bondMap.find(id); 

        // if it does not exist return nullptr
        if (location == _bondMap.end()) {
            return nullptr; 
        }

        // otherwise return the bond
        return location->second; 
    }

    //TODO: set up error messaging so it reports it through Nexus to avoid user confusion 
    template <typename T>
    void Nexus::bind(Entity e, T component) 
    { 
        _commandBuffer.defer_bind<T>(e, component); 
    }

    template <typename T>
    void Nexus::unbind(Entity e) 
    {  
        _commandBuffer.defer_unbind<T>(e); 
    }

    //TODO: set up error messaging so it reports it through Nexus to avoid user confusion 
    template <typename T>
    void Nexus::patch(Entity e, T component) 
    {
        // get the pool for that component type
        SparseSet<T>* pool = get_pool<T>();

        // replace that component 
        pool->replace(e, std::move(component)); 
    }

    template <typename T>
    T& Nexus::get(Entity e) 
    {
        // get the pool for that component type
        SparseSet<T>* pool = find_pool<T>();
        assert(pool != nullptr && "[Nexus] Get called for a component type with no registered pool."); 
        
        // return the component
        return pool->get(e); 
    }

    template <typename T>
    T* Nexus::try_to_get(Entity e) 
    {
        // get the pool for that component type
        SparseSet<T>* pool = find_pool<T>();
        if (pool == nullptr) return nullptr; 

        // return the component or nullptr
        return pool->try_to_get(e); 
    }

    template <typename T>
    bool Nexus::has(Entity e) 
    {
        // get the pool for that component type
        SparseSet<T>* pool = find_pool<T>();
        if (pool == nullptr) return false; 

        // return if it has the component
        return pool->has(e); 
    }

    template <typename... Components>
    SurveyHandle<Components...> Nexus::survey() 
    {   
        // create an array of SparseSet pointers per type packed into Components arguments
        //TODO: the solution to use get_pool instead of find_pool is to avoid passing a nullptr into Survey's constructor, consider this solution vs doing a null check in the constructor 
        std::array<SparseSetBase*, sizeof...(Components)> pools = { get_pool<Components>()... }; 
        
        // construct and return the survey 
        return SurveyHandle<Components...>(pools); 
    }

    template <typename... Components>
    BondHandle<Components...> Nexus::bond() 
    {
        // collect the component type ids of the passed in pools 
        std::array<ComponentTypeId, sizeof...(Components)> poolTypes = { get_component_type_id<Components>() ... }; 

        // create variables to determine whether the ids match a created bond or not 
        Bond* found = nullptr; // the matching bond if one exists (first one seen) 
        size_t mapped = 0; // how many ids already belong to a bond
        bool sameBond = true; // whether or not the requested ids belong to the same bond 

        // loop through the component types to determine if a bond already exists, or if it does not and can/cannot be created
        for (ComponentTypeId id : poolTypes) {

            // go to the next id if it is not associated with any bonds
            auto location = _bondMap.find(id); 
            if (location == _bondMap.end()) continue; 

            // if the id is associated with a bond increase mapped 
            mapped++; 

            // if found is still null set it so we can keep track of it in the future 
            if (found == nullptr) {

                found = location->second; 

            // if found is not associated with the currend id's bond it is not the sameBond
            } else if (found != location->second) {
                sameBond = false; 
            }
        }

        // if none of hte ids belong to a bond we can create a new one 
        if (mapped == 0) {

            // get the pools requested for the bond
            std::vector<SparseSetBase*> pools = { get_pool<Components>()... }; 
            
            // NOTE: we use a rew pointer and owned to create this unique pointer instead of make_unique
            // because the constructor is private and only accessible because of the friend class
            // and make_unique constructs the object inside as std::function that is not a friend of Bond

            // create the bond 
            std::unique_ptr<Bond> owned(new Bond(std::move(pools))); 

            // get the raw Bond pointer and put it in the bond map for every bonded component
            Bond* bondPtr = owned.get();
            for (ComponentTypeId id : poolTypes) {
                _bondMap[id] = bondPtr; 
            }

            // put the unique pointer in the bonds vector (leaves owned empty) 
            _bonds.push_back(std::move(owned)); 

            // return the BondHandle
            return BondHandle<Components...>(bondPtr); 
        } 

        // if every id belongs to the same Bond, we can just create the BondHandle
        if (mapped == sizeof...(Components) && sameBond && found->get_pool_count() == sizeof...(Components)) {
            return BondHandle<Components...>(found); 
        }

        // anything else means there is no bond and it can not be created because it shares a component pool with an existing bond 
        //TODO: integrate failure for this case with the logger (ideally it can be a compiler error before the user even runs code) 
        assert(false && "[Nexus] bond<>() was requested for components that are already bonded!");
        std::abort(); 
    }

    //--------------------------
    // Command Buffer Defintions
    //--------------------------

    template <typename T>
    void CommandBuffer::defer_bind(Entity e, T component) 
    {
        // create and push back the lambda for a bind command
        _bindQueue.push_back([this, e, component]() {
            _nexus.bind_immediate<T>(e, std::move(component)); 
        }); 
    }

    template <typename T>
    void CommandBuffer::defer_unbind(Entity e) 
    {
        // create and push back the lambda for an unbind command
        _unbindQueue.push_back([this, e]() {
            _nexus.unbind_immediate<T>(e); 
        }); 
    }
}
