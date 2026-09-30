/**
 * @file CommandBuffer.hpp
 * @author Shane David
 * @brief Defines the Command Buffer to solve undefined behavior resulting from modifying containers while iterating through them. 
 */

#pragma once 

#include <vector> // for storing the queues of deffered binds/unbinds/destroys

#include "Imago/ECS/Entity.hpp"
#include "Imago/ECS/Nexus.hpp"

namespace Imago::ECS 
{   

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

        /**
         * @brief go through all the defer queues and actually apply the operations to the associated Nexus. 
         * 
         */
        void flush(); 
    }; 
}