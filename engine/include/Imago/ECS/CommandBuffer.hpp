/**
 * @file CommandBuffer.hpp
 * @author Shane David
 * @brief Defines the Command Buffer to solve undefined behavior resulting from modifying containers while iterating through them. 
 */

#pragma once 

#include <vector> // for storing the queues of deffered binds/unbinds/destroys
#include <functional> // for std::function for type-erased std::function<void()>>

#include "Imago/ECS/Entity.hpp"

namespace Imago::ECS 
{   

    class Nexus; 

    /**
     * @brief Queues structural changes to apply all at once when there is no iteration going on. 
     * 
     * The Nexus will automatically route structual changes through the CommandBuffer when the user calls them during iteration. 
     * 
     * Modyfing containers while iterating through them should generally be avoided and causes undefined behavior. Becuase ImagoECS 
     * is contantly iterating over several containers, this becomes a big issue. The CommandBuffer is used for structural changes in component pools
     * (deleting Entity's, modyfing component data) to solve this problem. The CommandBuffer stores all changes in vectors and applies the operations 
     * in the order they were recored all at once when flush() is called. 
     * 
     */
    class CommandBuffer {
    private:

        Nexus& _nexus; ///> Reference to the Nexus the buffer will apply changes too. There will be one CommandBuffer per scene referencing that scene's Nexus
        std::vector<Entity> _destroyQueue; ///> Queue of Entity's that need to be destroyed 
        std::vector<std::function<void()>> _bindQueue; ///> Queue of the lambda functions to the bind commands that need to be executed
        std::vector<std::function<void()>> _unbindQueue; ///> Queue of the lambda functions to the unbind commands that need to be executed 

    public:
        
        /**
         * @brief Construct a new Command Buffer object with the Nexus that it will apply its queued operatoins to 
         * @param nexus The Nexus this buffer will flush its operations into. 
         */
        explicit CommandBuffer(Nexus& nexus); 

        /**
         * @brief Queues an Entity for destruction 
         * @param e The Entity to destroy 
         */
        void defer_destroy(Entity e); 

        // NOTE: we are storing our bind and unbind queues as type erased std::function<void()> storing lambda functions objects
        // that wrap a lambda with the logic necessary to bind/unbind/patch on specific component. This solves the problem that not every bind command is the same
        // and therefore cannot be stored in a std::vector since its type depends on what component is being bound. Wrapping them in a std::function<void()> erases
        // that type difference, allowing them to all be stored in a single vector. 

        /**
         * @brief Queues a bind_immediate command lambda 
         * @tparam T The component type that is being bound
         * @param e The entity to bind the component to
         * @param component The component data
         */
        template <typename T> 
        void defer_bind(Entity e, T component); 

        /**
         * @brief Queues an unbind_immediate command lambda
         * @tparam T The component type that is being unbound
         * @param e The entinty whose component of type T is being unbound 
         */
        template <typename T>
        void defer_unbind(Entity e); 

        /**
         * @brief go through all the defer queues and actually apply the operations to the associated Nexus. 
         */
        void flush(); 
    }; 

    // NOTE: Command Buffer definitions are located in Nexus.hpp. This is becuase of the circular include. C++ 
    // will not allow Imago/ECS/Nexus.hpp to be included in this file because this file is included in Nexus.hpp
    // this means we have to do an incomplete class definition instead of an include to get _nexus in the class
    // definition. However, we can not have the function definitions here becuase they actually call a Nexus 
    // method and therefore need the full definitions. And, since they are templated they cannot be in CommandBuffer.cpp either.
    // The most natural and least complicated place for them now is at the bottom of the Nexus.hpp file since the CommandBuffer
    // is intimately related to the Nexus. 
}