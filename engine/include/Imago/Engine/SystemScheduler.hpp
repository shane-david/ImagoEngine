/**
 * @file SystemScheduler.hpp
 * @author Shane David
 * @brief Defines the SystemScheduler, which runs scheduled systems in order every frame. 
 */

#pragma once

#include <cstdint> // for uint32_t 
#include <vector> // for the system lists 

namespace Imago::Engine 
{
    
    // NOTE: again, here we use a forward declaration because we do not need to know the full type of SceneContext
    struct SceneContext; 

    class SystemScheduler {
    public:
        
        /// @brief Function pointer signature for a scheduler system. 
        using SystemFn = void(*)(SceneContext&, float); 

    private:

        static constexpr float DEFAULT_FIXED_TIMESTEP = 1.0f / 60.0f; ///< 60 fixed steps per second 
        static constexpr uint32_t DEFAULT_MAX_FIXED_STEPS = 8; ///< 8 fixed steps (same default as Godot)

        float _accumulator = 0.0f; ///< real time not yet consumed by fixed steps
        float _fixedTimestep = DEFAULT_FIXED_TIMESTEP; ///< size of one fixed timestep in seconds
        uint32_t _maxFixedSteps = DEFAULT_MAX_FIXED_STEPS; ///< cap on fixed steps per tick
        SceneContext& _ctx; ///< reference to the SceneContext passed to every system
        
        std::vector<SystemFn> _coreSystems; ///< built-in per-frame systems shipped with ImagoEngine ran in a specific order
        std::vector<SystemFn> _userSystems; ///< user defined per-frame systems ran in a user-determined order
        std::vector<SystemFn> _fixedSystems; ///< fixed-timestep systems (could be core or user) 

        /**
         * @brief Adds requested system to the selected list unless it is already there.
         * 
         * Asserts that the system is not null. 
         * 
         * @param systems The list that should be added to. 
         * @param system The system to add to the list. 
         */
        static void add_system(std::vector<SystemFn>& systems, SystemFn system); 

        /**
         * @brief Removes requested system from selected list. 
         * 
         * @param systems List to remove the system from. 
         * @param system System to remove. 
         * @return true if something was removed. 
         */
        static bool remove_system(std::vector<SystemFn>& systems, SystemFn system); 

    public:

        /**
         * @brief Construct a new System Scheduler object
         * @param ctx Reference to SceneContext that should be passed to every system 
         */
        explicit SystemScheduler(SceneContext& ctx); 

        // explicityly disallow copy behavior
        SystemScheduler(const SystemScheduler&) = delete; 
        SystemScheduler& operator=(const SystemScheduler&) = delete; 

        /**
         * @brief Runs one frame of Scheduler systems. Called once per frame by the Scene. 
         * @param dt real time since the last frame, in seconds. 
         */
        void tick(float dt); 

        /**
         * @brief Registers a core per-frame system that should run before user systems
         * @param system The core system to register. 
         */
        void register_core(SystemFn system); 

        /**
         * @brief Registers a user per-frame system. 
         * @param system The user-defined system to register.
         */
        void register_update(SystemFn system);

        /**
         * @brief Registers a fiexed timestep system. Called with the fixed step, not the frame time. 
         * @param system The fixed system to register. 
         */
        void register_fixed(SystemFn system); 
        
        /**
         * @brief Removes a system from every list it is in. Warns if it was not registered anywhere. 
         * @param system The system to unregister. 
         */
        void unregister(SystemFn system); 

        //NOTE: This will mainly be used once we have editor integratoin so that the user does not have to change the order in code
        /**
         * @brief Set the user order object based off of a order vector they have arranged in the editor, 
         * @param order The order vector that the user systems should match. 
         */
        void set_user_order(std::vector<SystemFn> order); 

        /**
         * @brief Sets the fiexed step size in seconds. Must be greater than 0
         * @param timestep What the new timestep should be. 
         */
        void set_fixed_timestep(float timestep); 

        /**
         * @brief Sets the most fixed steps that can run in one tick. Must be greater than 0. 
         * @param maxSteps What the new max should be. 
         */
        void set_max_fixed_timestep(uint32_t maxSteps);

        /**
         * @brief Returns the fraction of the fiexed step left in the accumulator (0 to >1). For render interpolation. 
         */
        float get_alpha() const; 
    }; 
}