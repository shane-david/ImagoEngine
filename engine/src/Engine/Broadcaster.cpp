#include "Imago/Engine/Broadcaster.hpp"

#include <cassert>
#include <algorithm>

namespace Imago::Engine 
{

    Broadcaster::Broadcaster(SceneContext& ctx)
        : _ctx(ctx) { _broadcastDepth = 0; }

    std::vector<Broadcaster::Subscriber>::iterator Broadcaster::find_subscriber(std::vector<Subscriber>& subscribers, ErasedFn system) 
    {
        // look for the subscriber that shares the same address as system
        return std::find_if(subscribers.begin(), subscribers.end(), [system](const Subscriber& s){ return s.systemAddress == system; }); 
    }

    void Broadcaster::clear() 
    {
        //TODO: eventually have a system for this that just waits on the clear instead of crashing 
        // ensure ther are no broadcasts currently occuring 
        assert(_broadcastDepth == 0 && "[Broadcaster] you are trying to clear all subscriptions while the broadcaster is emitting!"); 

        // actually clear it 
        _subscribers.clear(); 
    }
} 