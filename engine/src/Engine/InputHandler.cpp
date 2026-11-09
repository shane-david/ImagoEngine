#include "Imago/Engine/InputHandler.hpp"

#include <cassert>
#include <spdlog/spdlog.h>
#include <algorithm> // for std::max
#include <SFML/Window/Event.hpp> // for handle_event()
#include <variant> // for get_if

namespace Imago::Engine
{
    /// @internal because it should only ever be used by this .cpp file
    namespace 
    {   
        /**
         * @brief apply the dead zone to a magnitude 
         * @param magnitude magnitude to rescale
         * @param deadZone dead zone to rescale by 
         * @return float rescaled magnitude 
         */
        float apply_dead_zone(float magnitude, float deadZone) 
        {
            // if, for some reason, the deadzone it 1.0f then it will never be pressed, treat it as so 
            if (deadZone >= 1.0f) return 0.0f; 

            // if the magnitude is less than the deadzone then treat is as it was not pressed
            if (magnitude <= deadZone) return 0.0f;

            // otherwise, rescale by taking shifting the value down by the deadzone and renormalize
            // NOTE: if the deadzone is 0.0f (like it would be for a non-axis input) this does nothing
            return (magnitude - deadZone) / (1.0f - deadZone); 
        }

        /**
         * @brief Returns whether or not an InputSource is a Gamepad source
         * @param source The InputSource to check
         */
        bool is_gamepad(const InputSource& source) 
        {
            return std::holds_alternative<GamepadButtonSource>(source) || std::holds_alternative<GamepadAxisSource>(source); 
        }

        /**
         * @brief How far below an input's threshold its release point sits. 
         * 
         * While an input is down it stays down until its value falls below the threshold - HYSTERESIS
         * so that a value hovering near the threshold cannot flick held on and off. 
         */
        constexpr float HYSTERESIS = 0.1f; 
    }

    // --- registering/adding actions ---
    
    ActionId InputHandler::register_action(const std::string& name) 
    {
        // TODO integrate assert into the logger
        // ensure the name is not empty
        assert(!name.empty() && "[InputHandler] cannot register an empty name!"); 

        // try to get the id, if it has already been added, warnd and return it 
        ActionId id = _state.action_id(name); 
        if (id != INVALID_ACTION) {
            spdlog::warn("[InputHandler] trying to register an action that has already been registered for '{}'!", name); 
            return id; 
        }

        // otherwise add the action
        return _state.add_action(name); 
    }

    void InputHandler::add_input(ActionId action, const InputSource& source, float deadZone, float threshold) 
    {
        // ensure that the action exits in _state before adding an input to it 
        if (_state.get(action) == nullptr) return; 

        //TODO intergrate asserts and warns into the logger

        //TODO move these to a helper method once SFML has similar Imago only enum wrappers
        // Unknown is not a real Key/GamepadButton/GamepadAxis input so guard against adding it
        // NOTE: this is especially important because so many variants of controllers exist so Unknown is much more common
        if (const auto* key = std::get_if<KeySource>(&source)) {
            assert(key->scancode != sf::Keyboard::Scancode::Unknown && "[InputHandler] cannot add Unknown as Key input!"); 
            if (key->scancode == sf::Keyboard::Scancode::Unknown) {
                spdlog::warn("[InputHandler] ignoring an attempt to add an Unknown key!");
                return; 
            }
        }
        if (const auto* gamepadButton = std::get_if<GamepadButtonSource>(&source)) {
            assert(gamepadButton->button != GamepadButton::Unknown && "[InputHandler] cannot add Unknown as a GamepadButton input!"); 
            if (gamepadButton->button == GamepadButton::Unknown) {
                spdlog::warn("[InputHandler] cannot add Unknown as a GamepadButton input!");
                return; 
            }
        }
        if (const auto* gamepadAxis = std::get_if<GamepadAxisSource>(&source)) {
            assert(gamepadAxis->axis != GamepadAxis::Unknown && "[InputHandler] cannot add Unknown as a GamepadAxis input!");
            if (gamepadAxis->axis == GamepadAxis::Unknown) {
                spdlog::warn("[InputHandler] cannot add Unknown as a GamepadAxis input!");
                return; 
            }
        }

        // use std::find_if to search for an entry with the same action and an equal source so we do not add the same input twice 
        auto it = std::find_if(_sources.begin(), _sources.end(), [action, &source](const SourceEntry& s) {
            return s.action == action && s.source == source; 
        }); 

        // if it was found decide whether to return silently or warn
        if (it != _sources.end()) {

            // if the deadZone and threshold already match, it is a harmless duplicate so stay silent and return early
            if (it->deadZone == deadZone && it->threshold == threshold) return; 

            //TODO intergrate warn into the logger
            // otherwise warn that a psuedo-duplicate was passed in and overwrite
            spdlog::warn("[InputHandler] input already added to this action, replacing its dead zone and threshold!"); 
            it->deadZone = deadZone; 
            it->threshold = threshold; 
            it->value = 0.0f;
            it->down = false;
            refresh_action(action); 
            return; 
        }

        // otherwise push back a source entry (down set to false by default)
        SourceEntry entry{source, action}; 
        entry.deadZone = deadZone;
        entry.threshold = threshold; 
        _sources.push_back(entry); 
    }

    void InputHandler::add_input(const std::string& name, const InputSource& source, float deadZone, float threshold)
    {
        // make sure the name is not an INVALID_ACTION
        ActionId id = _state.resolve(name); 
        if (id == INVALID_ACTION) return; 

        // otherwise forward to the id overload 
        add_input(id, source, deadZone, threshold); 
    }

    void InputHandler::add_input(const std::string& name, sf::Keyboard::Scancode scancode) 
    {
        // forward to InputSource overload with a KeySource built from the sf::Keyboard::Scancode
        add_input(name, KeySource{scancode}); 
    }

    void InputHandler::add_input(const std::string& name, sf::Mouse::Button button)
    {
        // forward to the InputSource overlaod with a MouseButtonSource build from the sf::Mouse::Buton
        add_input(name, MouseButtonSource{button}); 
    }

    void InputHandler::add_input(const std::string& name, GamepadButton button) 
    {
        // forward to the InputSource overload with a GamepadButtonSource built from the GamepadButton
        add_input(name, GamepadButtonSource{button}); 
    }

    void InputHandler::add_input(const std::string& name, GamepadAxis axis, float deadZone, float threshold) 
    {
        // forward to the InputSource overload with a GamepadAxisSource built from the GamepadAxis
        add_input(name, GamepadAxisSource{axis}, deadZone, threshold); 
    }

    // -- event handling ---

    void InputHandler::handle_event(const sf::Event& event) 
    {   
        // NOTE: we build a key/mouse source from the events code which is implicitly converted to an InputSource 
        // because of the std::variant, this allows us to have one method to check for an InputSource with an event
        
        // set_source_down accordingly for a key or mouse event, or release all for a focus lost event 
        if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
            set_source_value(KeySource{keyPressed->scancode}, 1.0f); 
        } else if (const auto* keyReleased = event.getIf<sf::Event::KeyReleased>()) {
            set_source_value(KeySource{keyReleased->scancode}, 0.0f); 
        } else if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
            set_source_value(MouseButtonSource{mousePressed->button}, 1.0f); 
        } else if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
            set_source_value(MouseButtonSource{mouseReleased->button}, 0.0f); 
        } else if (event.is<sf::Event::FocusLost>()) {
            release_all(); 
        }
    }

    void InputHandler::handle_gamepad_button(GamepadButton button, bool down)
    {
        // simply construct the source object and forward to set_source_value
        set_source_value(GamepadButtonSource{button}, down ? 1.0f : 0.0f ); 
    }

    void InputHandler::handle_gamepad_axis(GamepadAxis axis, float magnitude)
    {
        // simply construct the source object and forward to set_source_value
        set_source_value(GamepadAxisSource{axis}, magnitude); 
    }

    // --- private helpers ---

    void InputHandler::set_source_value(const InputSource& source, float magnitude)
    {   
        // look at every registered source 
        for (auto& entry : _sources) {

            // skip the entries with a different source than the one passed in
            if (entry.source != source) continue; 

            // apply the deadzone to the value so threshold compares against rescaled value (this does nothing for deadZone=0.0f default for non-axis inputs)
            const float value = apply_dead_zone(magnitude, entry.deadZone); 

            // NOTE: we skip sources with value == value so that repeated KeyPressed events fo a held key are ignored
            if (entry.value == value) continue; 

            // set the new value and down digital state based off of threshold and Hysterisis 
            entry.value = value; 

            // NOTE: if it is not down, just return whether or not the value is above the threshold, however if it is down we have to apply 
            // HYSTERESIS -> which is a band just below the threshold where down keeps its state so a value hovering near the threshold does not flicker it 
            entry.down = entry.down ? (value > std::max(0.0f, entry.threshold - HYSTERESIS)) : (value > entry.threshold); 

            // refresh the action now that the state has been updated 
            refresh_action(entry.action); 
        }
    }

    void InputHandler::refresh_action(ActionId action)
    {
        // start with a strength of 0 and a held value of false
        float strength = 0.0f; 
        bool held = false; 

        // loop over all SourceEntry with the corresponding action and grab the largest analog value for strength 
        // and whether or not any single input is past its own threshold for held 
        for (const auto& entry : _sources) {
            if (entry.action == action) {

                // take the value or the strength depending on which one is higher
                strength = std::max(strength, entry.value); 

                // logical or for down so any true-down makes it true
                held = held || entry.down; 
            }
        }

        // update both in the ActionState for that action
        _state.set_state(action, strength, held); 
    }

    void InputHandler::release_all()
    {
        // set down to false and value to 0 for every SourceEntry
        for (auto& entry : _sources) {
            entry.down = false; 
            entry.value = 0.0f; 
        }

        // refresh all of the actions 
        for (ActionId id = 0; id < _state._actions.size(); id++) {
            refresh_action(id); 
        }
    }

    void InputHandler::release_gamepad_inputs()
    {
        // set down to false and value to 0 for every SourceEntry that contains a Gamepad InputSource
        for (auto& entry : _sources) {
            if (is_gamepad(entry.source)) {
                entry.down = false;
                entry.value = 0.0f; 
            }
        }

        // refresh the gamepad actoins
        for (auto& entry : _sources) {
            if (is_gamepad(entry.source)) {
                refresh_action(entry.action); 
            }
        }
    }

    // --- forwards ---

    void InputHandler::clear_edges()
    {
        _state.clear_edges(); 
    }

    // --- getters ---

    InputState& InputHandler::state()
    {
        return _state; 
    }
}