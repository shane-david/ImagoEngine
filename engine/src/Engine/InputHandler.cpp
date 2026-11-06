#include "Imago/Engine/InputHandler.hpp"

#include <cassert>
#include <spdlog/spdlog.h>
#include <algorithm> // for std::max
#include <SFML/Window/Event.hpp> // for handle_event()
#include <variant> // for get_if

namespace Imago::Engine
{
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

    void InputHandler::add_input(ActionId action, const InputSource& source) 
    {
        // ensure that the action exits in _state before adding an input to it 
        if (_state.get(action) == nullptr) return; 

        //TODO intergrate assert into the logger
        // Unknown is not a real key so do not allow an entry to ver hold it 
        if (const auto* key = std::get_if<KeySource>(&source)) {
            assert(key->scancode != sf::Keyboard::Scancode::Unknown && "[InputHandler] cannot add Unknown as an input!"); 
            if (key->scancode == sf::Keyboard::Scancode::Unknown) {
                spdlog::warn("[InputHandler] ignoring an attempt to add an Unknown key!");
                return; 
            }
        }

        // use std::find_if to search for an entry with the same action and an equal source so we do not add the same input twice 
        auto it = std::find_if(_sources.begin(), _sources.end(), [action, &source](const SourceEntry& s) {
            return s.action == action && s.source == source; 
        }); 
        if (it != _sources.end()) return; 

        // otherwise push back a source entry (down set to false by default)
        _sources.push_back(SourceEntry{source, action, false}); 
    }

    void InputHandler::add_input(const std::string& name, const InputSource& source)
    {
        // make sure the name is not an INVALID_ACTION
        ActionId id = _state.resolve(name); 
        if (id == INVALID_ACTION) return; 

        // otherwise forward to the id overload 
        add_input(id, source); 
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

    // -- event handling ---

    void InputHandler::handle_event(const sf::Event& event) 
    {   
        // NOTE: we build a key/mouse source from the events code which is implicitly converted to an InputSource 
        // because of the std::variant, this allows us to have one method to check for an InputSource with an event
        
        // set_source_down accordingly for a key or mouse event, or release all for a focus lost event 
        if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
            set_source_down(KeySource{keyPressed->scancode}, true); 
        } else if (const auto* keyReleased = event.getIf<sf::Event::KeyReleased>()) {
            set_source_down(KeySource{keyReleased->scancode}, false); 
        } else if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
            set_source_down(MouseButtonSource{mousePressed->button}, true); 
        } else if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
            set_source_down(MouseButtonSource{mouseReleased->button}, false); 
        } else if (event.is<sf::Event::FocusLost>()) {
            release_all(); 
        }
    }

    // --- private helpers ---

    void InputHandler::set_source_down(const InputSource& source, bool down)
    {   
        // look at every registered source 
        for (auto& entry : _sources) {

            // if the source exists and has not alredy been set, set it and refresh it 
            // NOTE: we skip sources with down == down so that repeated KeyPressed events fo a held key are ignored
            if (entry.source == source && entry.down != down) {
                entry.down = down; 
                refresh_action(entry.action); 
            }
        }
    }

    void InputHandler::refresh_action(ActionId action)
    {
        // start with a strength of 0 
        float strength = 0.0f; 

        // loop over all SourceEntry with the corresponding action and set the strenght to the highest one 
        for (const auto& entry : _sources) {
            if (entry.action == action) {

                // if the entry is down, the strenght should be 1
                const float value = entry.down ? 1.0f : 0.0f; 

                // take the value or the strength depending on which one is higher
                strength = std::max(strength, value); 
            }
        }

        // update the strength in the ActionState for that action
        _state.set_strength(action, strength); 
    }

    void InputHandler::release_all()
    {
        // set down to false for every SourceEntry
        for (auto& entry : _sources) {
            entry.down = false; 
        }

        // refresh all of the actions 
        for (ActionId id = 0; id < _state._actions.size(); id++) {
            refresh_action(id); 
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