#include "Imago/Engine/EngineControl.hpp"

#include <cassert>
#include <spdlog/spdlog.h>
#include <utility> 

namespace Imago::Engine 
{
    // scene switching 

    void EngineControl::switch_scene(const std::string& name, bool persist) 
    {
        //TODO integrate assert with the logger

        // ensure that the name is not empty 
        assert(!name.empty() && "[EngineControl] cannot switch to a scene without a name!"); 
        if (name.empty()) {
            spdlog::warn("[EngineControl] cannot switch to a scene without a name!");
            return; 
        }

        // build a switch request
        _pendingSwitch = SwitchRequest{name, persist}; 
    }

    // NOTE: std::option is a C++ class that manages a value that may ore may not be present. It is used here 
    // because we want to return the pending switch whenever it is called, but there might not be one. We use 
    // std::exchagne her to put a std::nullopt into _pendingSwitch once we return it so that we do not keep 
    // a switch pending once it has been requested 

    std::optional<EngineControl::SwitchRequest> EngineControl::take_switch_request() 
    {
        return std::exchange(_pendingSwitch, std::nullopt); 
    }

    // quitting
    
    void EngineControl::quit() 
    {
        _quitRequested = true; 
    }

    bool EngineControl::quit_requested() const
    {
        return _quitRequested; 
    }
}