#include "Imago/ECS/CommandBuffer.hpp"
#include "Imago/ECS/Nexus.hpp"

namespace Imago::ECS 
{

    // constructor
    CommandBuffer::CommandBuffer(Nexus& nexus)
        : _nexus(nexus) { }
    
    void CommandBuffer::defer_destroy(Entity e) 
    {
        // put the entity in the queue
        _destroyQueue.push_back(e); 
    }

    void CommandBuffer::flush() 
    {   
        //NOTE the order: we bind then unbind then destroy to avoid any errors of multiple queues existing for the same entity

        // bind command queue, call the lambda command for all bind actions
        for (auto& command : _bindQueue) {
            command(); 
        }
        _bindQueue.clear(); 

        // unbind command queue, call the lambda command for all unbind actions
        for (auto& command : _unbindQueue) {
            command(); 
        }
        _unbindQueue.clear(); 

        // destroy queue, destroy immediate the entities in the queue and clear it 
        for (Entity e : _destroyQueue) {
            _nexus.destroy_immediate(e); 
        }
        _destroyQueue.clear(); 
    }
}