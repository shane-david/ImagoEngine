#include "Imago/Engine/SystemScheduler.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <spdlog/spdlog.h>
#include <utility>

namespace Imago::Engine 
{   
    SystemScheduler::SystemScheduler(SceneContext& ctx) 
        : _ctx(ctx) {}

    // method for tick and fixed timestep, call each system once every frame or each fixed system for each fixed timestep
    void SystemScheduler::tick(float dt) 
    {
        // make sure dt is a valid timestep 
        assert(dt >= 0.0f && "[SystemScheduler] tick called with a negative dt!"); 

        // progress the accumulator
        _accumulator += dt; 

        // call the fixed systems while there is a step's worth of time in the accumulator and maxSteps has not been reached
        std::uint32_t steps = 0; 
        while (_accumulator >= _fixedTimestep && steps < _maxFixedSteps) {
            for (SystemFn system : _fixedSystems) {
                system(_ctx, _fixedTimestep); 
            }

            // remove a step out of the accumulator and progress steps
            _accumulator -= _fixedTimestep;
            steps++; 
        }

        // if the loop is stopping because of the cap, log a warning and discard the backlog in the accumulator 
        if (_accumulator >= _fixedTimestep) {
            //TODO log a warning with the logger
            _accumulator = std::fmod(_accumulator, _fixedTimestep); 
        }

        // call the core systems
        for (SystemFn system : _coreSystems) {
            system(_ctx, dt); 
        }

        // call the user systems
        for (SystemFn system : _userSystems) {
            system(_ctx, dt); 
        }
    }

    // methods for adding and removing systems

    void SystemScheduler::add_system(std::vector<SystemFn>& systems, SystemFn system) 
    {
        //TODO integrate this assert into the logger
        // make sure that the system is not null
        assert(system != nullptr && "[SystemScheduler] Trying to add a system that does not exist!"); 

        // ensure that the system is not already in the list using std::find
        auto it = std::find(systems.begin(), systems.end(), system); 
        if (it != systems.end()) return; 

        // otherwise add it to the list at the back 
        systems.push_back(system); 
    }

    bool SystemScheduler::remove_system(std::vector<SystemFn>& systems, SystemFn system) 
    {
        // remove the element with std::remove to shift non-removed items forward and then erase from the new end
        auto newEnd = std::remove(systems.begin(), systems.end(), system); 

        // before we erase check if the new end actually changed (if it did not nothing was removed)
        bool wasRemoved = (newEnd != systems.end()); 

        // erase from the new end
        systems.erase(newEnd, systems.end()); 

        return wasRemoved; 
    }

    // un/register methods 
    void SystemScheduler::register_core(SystemFn system) { add_system(_coreSystems, system); }
    void SystemScheduler::register_update(SystemFn system) { add_system(_userSystems, system); }
    void SystemScheduler::register_fixed(SystemFn system) { add_system(_fixedSystems, system); }

    void SystemScheduler::unregister(SystemFn system) 
    {   
        bool removed = false; 

        // remove from all systems and set removed accordingly 

        // NOTE: here |= (bitwise or assignment operator) is used because if we just used a regular ||, a true
        // would short circuit and skip the remaining remove_system calls, if we just assigned, removed would 
        // only hold the result of the last system 

        removed |= remove_system(_coreSystems, system);
        removed |= remove_system(_userSystems, system); 
        removed |= remove_system(_fixedSystems, system); 

        // TODO, figure out this warning through the logger once it exists
        // log it if nothing was removed 
        if (!removed) {
            spdlog::warn("[SystemScheduler] Tried to unregister a system that has not been registered"); 
        }
    }

    // setters

    void SystemScheduler::set_user_order(std::vector<SystemFn> order) 
    {   
        // TODO: integrate theses asserts into the logger

        // make sure the user order vector has the correct amount of elements
        assert(order.size() == _userSystems.size() && "[SystemScheduler] trying to set user order with an incomplete vector!"); 

        // make sure the user order vector has the same systems as _userSystems
        assert(std::is_permutation(order.begin(), order.end(), _userSystems.begin())); 

        // if we made it this far, move the new order into the correct vector
        _userSystems = std::move(order); 
    }

    void SystemScheduler::set_fixed_timestep(float timestep) 
    {
        // TODO: integrate this assert into the logger
        // make sure the timestep is a valid value
        assert(timestep > 0.0f && "[SystemSchduler] trying to set timestep to an invalid value!"); 

        // set the timestep
        _fixedTimestep = timestep; 
    }

    void SystemScheduler::set_max_fixed_steps(uint32_t maxSteps) 
    {
        // TODOL integrate this assert into the logger
        // make sure the max steps is a valid value
        assert(maxSteps > 0 && "[SystemScheduler] trying to set max timestep to an invalid value!"); 

        // set the max timestep
        _maxFixedSteps = maxSteps; 
    }

    // getters

    float SystemScheduler::get_alpha() const { return _accumulator / _fixedTimestep; }


}