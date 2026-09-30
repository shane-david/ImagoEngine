#include "Imago/ECS/CommandBuffer.hpp"

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

        // destroy queue, destroy immediate the entities in the queue and clear it 
        for (Entity e : _destroyQueue) {
            _nexus.destroy_immediate(e); 
        }
        _destroyQueue.clear(); 
    }
}