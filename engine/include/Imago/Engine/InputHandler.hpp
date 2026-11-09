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
        
        //TODO have different vectors for differnt entries to get rid of the InputSource variant and unecessary data for non-axis inputs. 

        /// @brief One raw input added to an action, whether or not it is currently down, its magnitude value, and data for axes dead zones and thresholds. 
        struct SourceEntry {
            InputSource source; ///< the raw input
            ActionId action; ///< the action the input drives
            bool down = false; ///< current digital state of the raw input 
            float value = 0.0f; ///< current analog magnitude (0-1) of the input. Drives strength
            float deadZone = 0.0f; ///< how much axis travel nearest the center counts as not moved. Everything past it is rescaled to 0-1. Key/Mouse defualt -> axes provide their own.
            float threshold = 0.5f; ///< minimum magnitude axis needs to be pressed before it counts as being down. Key/Mouse default -> axes provide their own. 
        }; 

        InputState _state; ///< action names and states, what systems query through SceneContext
        std::vector<SourceEntry> _sources; ///< every input of every action and whether or not it is down

        // --- private methods --- 

        /**
         * @brief Takes magnitude of an input and peforms dead zone, thershold, and hysterisis math to set the value accordingly determine whether or not it is down. 
         * @param source InputSource to set the value of 
         * @param magnitude magnitude of that input
         */
        void set_source_value(const InputSource& source, float magnitude); 

        /**
         * @brief Recomputes an action's strength based off its analog value and its held state based off its down digital value
         * and then sets those to the corresponding ActionState in _state.
         */
        void refresh_action(ActionId action); 

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
        InputHandler(InputHandler&&) = delete;
        InputHandler& operator=(InputHandler&&) = delete; 

        /**
         * @brief Registers an action name and returns its id. 
         * 
         * Registering an existing action warns and returns its existing id. 
         * @param name Name of the action to register. 
         */
        ActionId register_action(const std::string& name); 

        /// @brief Adds a raw input to an action. Overloaded so callers can call with action name or action id and InputSource wrapper or SFML object. 
        void add_input(ActionId action, const InputSource& source, float deadZone = 0.0f, float threshold = 0.5f); 
        void add_input(const std::string& name, const InputSource& source, float deadZone = 0.0f, float threshold = 0.5f); 
        void add_input(const std::string& name, sf::Keyboard::Scancode scancode); 
        void add_input(const std::string& name, sf::Mouse::Button button); 
        void add_input(const std::string& name, GamepadButton button);
        void add_input(const std::string& name, GamepadAxis axis, float deadZone = 0.2f, float threshold = 0.5f); 

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
         * @brief Handles a Gamepad button event. Called by Game which determines what button the event is associated with, 
         * and what state it is in. 
         * @param button The Gamepad button - turned from an SDL2 enum to an Imago enum by Game.
         * @param down whether or not the button is down
         */
        void handle_gamepad_button(GamepadButton button, bool down);

        /**
         * @brief Handles a Gamepad axis event. Called by Game which determines which axis had motion 
         * and what the magnitude of that motion was. 
         * @param axis The Gamepad axis that had motion - turned from an SDL2 enum to an Imago enum by Game. 
         * @param magnitude the magnitude of that motion for that axis. 
         */
        void handle_gamepad_axis(GamepadAxis axis, float magnitude); 

        /**
         * @brief Releases all Gamepad inputs, called by Game when a controller is disconnected. 
         */
        void release_gamepad_inputs(); 

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