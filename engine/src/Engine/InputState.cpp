#include "Imago/Engine/InputState.hpp"

#include <cassert>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <glm/geometric.hpp> // for glm::length
#include <utility> // for the as_const cast

namespace Imago::Engine
{

    ActionId InputState::add_action(const std::string& name) 
    {
        //TODO integrate asserts into logger 
        // ensure the name is not empty 
        assert(!name.empty() && "[InputState] cannot add an action with an empty name!"); 

        // if the name already exists just return its existing id 
        auto it = _ids.find(name); 
        if (it != _ids.end()) {
            return it->second; 
        }

        // otherwise add the action, get the id (size of actions), store the name, and then push a default ActionState
        ActionId id = static_cast<ActionId>(_actions.size()); 
        _ids[name] = id; 
        _actions.push_back(ActionState()); 

        return id; 
    }

    // --- action state setters  ---

    void InputState::set_state(ActionId action, float strength, bool held)
    {
        //TODO: integrate assert into logger

        // ensure that there was not a strenght of 0 passed in with a held state of true
        assert(!(held && strength <= 0.0f) && "[InputState] held connaot be true with no strength!"); 
        
        // if the id is valid, get its corresponding ActionState
        ActionState* state = get(action); 
        if (!state) return; 

        // clamp the strength between 0 and 1 
        strength = std::clamp(strength, 0.0f, 1.0f); 
        state->strength = strength; 

        // store the old held result and update the new one based on the passed in param
        bool wasHeld = state->held; 
        state->held = held; 

        // NOTE: here we set edges for both frame and fixed because the state does not know which kind of system will read next.
        // So, instead the phase tracking decided which query reads, and the hooks decide when each set clears. 

        // set began and ended states for both frame and fixed 
        if (!wasHeld && state->held) {
            state->beganFrame = true;
            state->beganFixed = true; 
        } else if (wasHeld && !state->held) {
            state->endedFrame = true;
            state->endedFixed = true; 
        }
    }

    void InputState::clear_edges()
    {
        // set all four edge flags to false for every action 
        for (auto& action : _actions) {
            action.beganFrame = false;
            action.beganFixed = false;
            action.endedFrame = false;
            action.endedFixed = false; 
        }
    }

    // --- action state getters ---

    const InputState::ActionState* InputState::get(ActionId action) const
    {
        //TODO integrate asserts/warnings into logger
        // validate the id (make sure it is smaller than _actions.size())
        assert(action < _actions.size() && "[InputState] trying to get an ActionState for an invalid ActionId!");
        if (action >= _actions.size()) {
            spdlog::warn("[InputState] trying to get an ActionState for an invalid ActionId!"); 
            return nullptr; 
        }

        // otherwise return the pointer to the action state 
        return &_actions[action]; 
    }

    InputState::ActionState* InputState::get(ActionId action) 
    {
        return const_cast<ActionState*>(std::as_const(*this).get(action)); 
    }

    // --- action id getters ---

    // NOTE: there is a difference here between resolve (which gets the ActionId and asserts) and action_id which just returns it
    // even if it is invalid. action_id is public and should be able to be used by calles without being punished for an answer. 
    // however, when the inner class queires it needs to know whether or not the id exits without verifying it every time. 

    ActionId InputState::resolve(const std::string& name) const 
    {
        // get the action id from the name
        ActionId id = action_id(name); 

        //TODO integrate into the logger 
        // if it is INVALID_ACTIon assert/warn 
        assert(id != INVALID_ACTION && "[InputState] invalid ActionId!");
        if (id == INVALID_ACTION) {
            spdlog::warn("[InputState] invalid ActionId for '{}'!", name); 
        }

        return id; 
    }

    ActionId InputState::action_id(const std::string& name) const
    {
        // try to look up the name in ids, if it is not found return INVALID_ACTION, otherwise return it 
        auto it = _ids.find(name); 
        if (it == _ids.end()) {
            return INVALID_ACTION; 
        } else {
            return it->second; 
        }
    }

    // --- scheduler hooks ---

    void InputState::begin_frame_phase() 
    {
        _phase = Phase::Frame; 
    }

    void InputState::end_frame_phase() 
    {
        // clear every actions frame edges
        for (auto& action : _actions) {
            action.beganFrame = false;
            action.endedFrame = false; 
        }

        // set phase back to frame 
        _phase = Phase::Frame; 
    }

    void InputState::begin_fixed_step()
    {
        _phase = Phase::Fixed; 
    }

    void InputState::end_fixed_step()
    {
        // clear every actions fixed edges
        for (auto& action : _actions) {
            action.beganFixed = false;
            action.endedFixed = false; 
        }

        // set phse back to frame 
        _phase = Phase::Frame; 
    }

    // -- query methods --- 

    bool InputState::held(ActionId action) const
    {
        // get the ActionState, if it is null return the default 
        const ActionState* state = get(action); 
        if (!state) return false; 

        // otherwise return held flag 
        return state->held; 
    }

    bool InputState::held(const std::string& name) const
    {
        // get the id, if it is INVALID_ACTION, return the default 
        ActionId id = resolve(name);
        if (id == INVALID_ACTION) return false; 

        // otherwise foward to the id overload
        return held(id); 
    }

    bool InputState::began(ActionId action) const
    {
        // get the ActionState, if it is null return the default 
        const ActionState* state = get(action); 
        if (!state) return false; 

        // otherwise return the began flag depending on the current phase 
        return (_phase == Phase::Fixed) ? state->beganFixed : state->beganFrame; 
    }

    bool InputState::began(const std::string& name) const
    {
        // get the id, if it is INVALID_ACTION, return the default 
        ActionId id = resolve(name);
        if (id == INVALID_ACTION) return false; 

        // otherwise forward to the id overload
        return began(id); 
    }

    bool InputState::ended(ActionId action) const
    {
        // get the ActionState, if it is null return the default 
        const ActionState* state = get(action); 
        if (!state) return false; 
        
        // otherwise return the ended flag depending on the current phase
        return (_phase == Phase::Fixed) ? state->endedFixed : state->endedFrame; 
    }

    bool InputState::ended(const std::string& name) const
    {
        // get the id, if it is INVALID_ACTION, return the default 
        ActionId id = resolve(name);
        if (id == INVALID_ACTION) return false; 

        // otherwise forward to the id overload
        return ended(id); 
    }

    float InputState::strength(ActionId action) const
    {
        // get the ActionState, if it is null return the default 
        const ActionState* state = get(action); 
        if (!state) return 0.0f; 

        // otherwise return the strength 
        return state->strength; 
    }

    float InputState::strength(const std::string& name) const
    {
        // get the id, if it is INVALID_ACTION, return the default 
        ActionId id = resolve(name);
        if (id == INVALID_ACTION) return 0.0f; 

        // otherwise forward to the id overload 
        return strength(id); 
    }

    float InputState::axis(ActionId negative, ActionId positive) const
    {
        return strength(positive) - strength(negative); 
    }

    float InputState::axis(const std::string& negative, const std::string& positive) const
    {
        // get both ids, if either are INVALID_ACTION, return the default 
        ActionId negId = resolve(negative);
        ActionId posId = resolve(positive); 
        if (negId == INVALID_ACTION || posId == INVALID_ACTION) return 0.0f; 

        // otherwise forward to the id overload
        return axis(negId, posId); 
    }

    glm::vec2 InputState::direction(ActionId left, ActionId right, ActionId up, ActionId down) const
    {
        // build the raw direction from two axes (for SFML coordinates y points down)
        glm::vec2 result(axis(left, right), axis(up, down)); 

        // clamp the length to at most 1 so a keyboard diagonal is not faster 
        const float length = glm::length(result); 
        if (length > 1.0f) {
            result /= length; 
        }

        return result; 
    }

    glm::vec2 InputState::direction(const std::string& left, const std::string& right, const std::string& up, const std::string& down) const
    {
        // get all 4 ids, if any are INVALID_ACTION, return the default 
        ActionId leftId = resolve(left), rightId = resolve(right), upId = resolve(up), downId = resolve(down); 
        if (leftId == INVALID_ACTION || rightId == INVALID_ACTION || upId == INVALID_ACTION || downId == INVALID_ACTION) return glm::vec2(0.0f); 

        // otherwise forward to the id overload
        return direction(leftId, rightId, upId, downId); 
    }
}