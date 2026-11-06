/**
 * @file InputHandler.hpp
 * @author Shane David
 * @brief Defines InputHandler, which owns te inputs and the InputState and turns raw events into action state. 
 */

#pragma once

#include <vector> // to store the input sources 
#include <string> // to store names of registered actions

#include "Imago/Engine/InputSource.hpp"
#include "Imago/Engine/InputState.hpp"

// forward declaration for sf::Event (necessary for polling)
namespace sf { class Event; }

namespace Imago::Engine
{

    /**
     * @brief Owned by Game. Handles registering actions and adding raw inputs to them. It updates InputState based off of window events.
     * Systems never see this class, instead they see the InputState it owns. An action has inputs that the user decides but all logic within states 
     * should be based off of whether or not the action has been executed, not the input. Mutiple inputs can drive a single action.
     */
    class InputHandler {
    private:
        
        /// @brief One raw input added to an action and whether or not it is currently down.
        struct SourceEntry {
            InputSource source; ///< the raw input
            ActionId action; ///< the action the input drives
            bool down = false; ///< current state of the raw input 
        }; 

        InputState _state; ///< action names and states, what systems query through SceneContext
        std::vector<SourceEntry> _sources; ///< every input of every action and whether or not it is down

        // --- private methods --- 

        /**
         * @brief Sets the down flag on every entry matching a source, then recomputes the affected actions. 
         */
        void set_source_down(const InputSource& source, bool down); 

        /**
         * @brief Recomputes an action's strength from its inputsand writes to its state. 
         */
        void refresh_actions(ActionId action); 

        /**
         * @brief Releases every input, used on focus loss. 
         */
        void release_all(); 

    public:
        
        /// @brief Constructs and InputHandler object. 
        InputHandler() = default; 

        // explicitly disallow copy behavior
        InputHandler(const InputHandler&) = delete; 
        InputHandler& operator=(const InputHandler&) = delete;
        InputHandler(const InputHandler&&) = delete;
        InputHandler& operator=(InputHandler&&) = delete; 

        /**
         * @brief Registers an action name and returns its id. 
         * 
         * Registering an existing action warns and returns its existing id. 
         * @param name Name of the action to register. 
         */
        ActionId register_action(const std::string& name); 

        /// @brief Adds a raw input to an action. Overloaded so callers can call with action name or action id and InputSource wrapper or SFML object. 
        void add_input(const std::string& action, const InputSource& source); 
        void add_input(ActionId action, const InputSource& source); 
        void add_input(const std::string& action, sf::Keyboard::Key key); 
        void add_input(const std::string& action, sf::Mouse::Button button); 

        //TODO remove_input once there is a rebinding menu 

        /**
         * @brief Updates input and action state from one window event. Handles key and mouse button presses and releases, 
         * and focus loss. 
         * 
         * Repeataed KeyPressed events for a key that is already down are ignored. 
         * @param event The SFML event
         */
        void handle_event(const sf::Event& event); 

        /**
         * @brief Clears latched edges, keeps held state. Game calls this after a scene switch. 
         */
        void clear_edges(); 

        /**
         * @brief Returns the state that SceneContext::input points to. 
         */
        InputState& state(); 
    }; 
}