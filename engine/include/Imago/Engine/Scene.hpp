/**
 * @file Scene.hpp
 * @author Shane David
 * @brief Defines the Scene, the abstract world that owns a Nexus, Broadcaster, and SystemScheduler
 */

#pragma once

#include "Imago/ECS/Nexus.hpp"
#include "Imago/Engine/Broadcaster.hpp"
#include "Imago/Engine/SystemScheduler.hpp"
#include "Imago/Engine/SceneContext.hpp"

namespace Imago::Engine 
{

    class Scene {
    protected:
        
        // NOTE: these members are protected because they have to be defined in the children class of Scene but should not be called
        // directly, instead load and unload should be called directly so that they can force a flush after the user-defined logic

        /**
         * @brief Called by `load()`. Behavior is defined in user definitions of Scenes were the user can create entities, register systems, and subscribe to events. 
         */
        virtual void on_load() = 0; 

        /**
         * @brief Called by `unload()`. This is optional in user defintions of Scenes because most clean-up is handled automatically but any additional
         * clean up that ought to happen during the termination of a Scene should go here. 
         */
        virtual void on_unload() {} 

        // protected members of the ECS/Engine objects that a Scene owns, can only be acccessed in children of Scene
        ECS::Nexus _nexus; ///< scene's Nexus
        SceneContext _ctx; ///< scene's own copy of the context passed into every system
        Broadcaster _broadcaster; ///<  scene's event bus
        SystemScheduler _scheduler; ///< runs this scene's scheduled systems

    public:
        
        /**
         * @brief Construct a new Scene object and copies the game's persistent context and filles the 
         * remaining pointers with this scenes instance of the Nexus and Broadcaster
         * @param gameCtx Game's SceneContext with global Game objects filled in. 
         */
        explicit Scene(const SceneContext& gameCtx); 

        // need a default destructor because this scene is abstract
        virtual ~Scene() = default; 

        // explicitly dissalow copy behaviour because the context holds pointers into this object 
        Scene(const Scene&) = delete; 
        Scene& operator=(const Scene&) = delete; 
        Scene(Scene&&) = delete; 
        Scene& operator=(Scene&&) = delete; 

        /**
         * @brief Loads  the scene. Runs the user-defined on_load() method then flushes everything so they actually go through before the first frame of execution. 
         */
        void load(); 

        /**
         * @brief Unloads the scene. Runs the user-defined on_unload() method then flushes everything. 
         */
        void unload(); 

        /**
         * @brief Runs one frame, ticks the scheduler, then flushes the Nexus. 
         * @param dt Real time since the last frame, in seconds. 
         */
        void update(float dt); 

        //TODO: render method
    }; 

}