/**
 * @file InputState.hpp
 * @author Shane David
 * @brief Defines InputState, the read-only view of every action that sytems query. 
 */

#pragma once

#include <cstdint> // for unit32_t for ActionId
#include <vector> // for storing ActionStates
#include <unordered_map> // for storing string to action map
#include <string> // for action names 
#include <glm/vec2.hpp> // for directional returns 

namespace Imago::Engine
{
    /// @brief Id of a registered action, Resolve a name once with action_id() and cache it for the fast path. 
    using ActionId = uint32_t; 

    /// @brief The bits that should be returned by action_id() when a name is not registered. 
    static constexpr ActionId INVALID_ACTION = UINT32_MAX; 

    /**
     * @brief What systems query for input (ctx.input->began("jump")). This is only for querying. The InputHandler writes it. 
     * 
     * Edges (began/ended) are phase aware. In a per-fram system they are true for the frame the input arrived or left in.
     * In a fixed system they are true for the first fixed step after it arrived or left. 
     */
    class InputState {
    private:
        
        // friend classes, the InputHandler needs to write actions and the scheduler needs to tell the state which phase is running 
        friend class InputHandler; 
        friend class SystemScheduler; 

        friend class InputStateTestAccess; //TODO only for test access, remove in release 

        /// @brief Which kind of system is currently running so that we know which edge flags queries should read. 
        enum class Phase { Frame, Fixed }; 

        struct ActionState {
            float strength = 0.0f; ///< 0 to 1, largest of its inputs. 
            bool held = false; ///< true if strength is above 0
            bool beganFrame = false; ///< began edge for per-frame systems, cleared after them each frame
            bool endedFrame = false; ///< ended edge for per-frame systems
            bool beganFixed = false; ///< began edge for fixed systems, latched until a fixed step has run
            bool endedFixed = false; ///< ended edge for fixed systems
        }; 

        // --- input storage data --- 

        std::vector<ActionState> _actions; ///< states of all registered actions, indexed by ActionId
        std::unordered_map<std::string, ActionId> _ids; ///< action name to id
        Phase _phase = Phase::Frame; ///< current phase

        // --- private methods ---

        /**
         * @brief Registers an action name and returns its id. An existing name returns the existing id. 
         * @param name Name of the action to register. 
         * @return ActionId id of the registered action. 
         */
        ActionId add_action(const std::string& name);

        /**
         * @brief Set an action's strength, derives held, and the latches began/ended edges when held changes. 
         * @param action The action to set the strength for. 
         * @param strength The value to insert as the action's strength. 
         */
        void set_strength(ActionId action, float strength); 

        /**
         * @brief Clears every latched edge but leaves held and strength alone. 
         * 
         * Used on Scene switches. 
         */
        void clear_edges(); 

        /**
         * @brief Returns the action state for an id. Returns nullptr if the id is not valid. 
         * @param action The id of the action to get the state for. 
         */
        const ActionState* get(ActionId action) const; 

        // --- scheduler hooks, called from SystemScheduler::tick() ---

        /// @brief called when per-frame systems are about to run
        void begin_frame_phase();

        /// @brief called when per-frame systems are done, clears the frame edges. 
        void end_frame_phase();

        /// @brief called when one fixed step is about to run
        void begin_fixed_step();

        /// @brief called when that fixed step is done, clears the fixed edges it has consumed. 
        void end_fixed_step(); 

    public: 
        
        /// @brief Construct a new InputState object
        InputState() = default; 

        // explicity disallow copy and move behavior
        InputState(const InputState&) = delete; 
        InputState& operator=(const InputState&) = delete;
        InputState(const InputState&&) = delete;
        InputState& operator=(InputState&&) = delete; 

        /**
         * @brief Returns an action id from its name. Returns INVALID_ACTION for unregisterd names. Used to cache ids. 
         * @param name Name of the action you want the id for. 
         */
        ActionId action_id(const std::string& name) const; 

        // --- user api methods ---

        /// @brief returns true while the action is being held down. Overloaded so it can be called with an action's name or id. 
        bool held(const std::string& action) const; 
        bool held(ActionId action) const; 

        /// @brief returns true when the action started being held. Overloaded so it can be called with an action's name or id. 
        bool began(const std::string& action) const; 
        bool began(ActionId action) const; 

        /// @brief returns true when the action stopped being held. Overloaded so it can be called with an action's name or id. 
        bool ended(const std::string& action) const; 
        bool ended(ActionId action) const; 

        /**
         * @brief returns how much the action is engaged, from 0 to 1. Overloaded so it can be called with an action's name or id.
         * 
         * Keyboard and mouse only give 0 or 1. If there are multiple bindins the largest one wins. 
         */
        float strength(const std::string& action) const;
        float strength(ActionId action) const; 

        /**
         * @brief Returns result of a direction from one line of input, opposite inputs of the same strength cancel out. Overloaded so it can be called with an action's name or id. 
         * 
         * strength(positive) - strength(negative)
         */
        float axis(const std::string& negative, const std::string& positive) const;
        float axis(ActionId negative, ActionId positive) const; 

        /**
         * @brief Returns result of a direction in 2D from four lines of input. Overloaded so it can be called with an action's name or id. 
         * 
         * two axes cobined into a direction with length at most 1, so a keyboard diagonal is not faster. 
         */
        glm::vec2 direction(const std::string& left, const std::string& right, const std::string& up, const std::string& down) const; 
        glm::vec2 direction(ActionId left, ActionId right, ActionId up, ActionId down); 
    }; 
}