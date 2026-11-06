/**
 * @file InputSource.hpp
 * @author Shane David
 * @brief Defines the raw inputs that drive an action
 */

#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <variant> 

namespace Imago::Engine 
{
    /// @brief A keybaord key that drives an action. Reads as 0 or 1. (pressed or not pressed)
    struct KeySource { sf::Keyboard::Scancode scancode; }; 

    /// @brief A mouse button that drives an action. Reads as 0 or 1. (pressed or not pressed)
    struct MouseButtonSource { sf::Mouse::Button button; }; 

    //TODO: do the same for GamepadButtonSource and GampadAxisSource with SDL2 integration 

    // NOTE: std::variant's == operator is used by the InputHandler to find matching inputs. It only exists if every alternative 
    // of the variant has its own == operator. The variant compares the held type first, so a KeySource never equals a MouseButtonSource.

    constexpr bool operator==(const KeySource& a, const KeySource& b) { return a.scancode == b.scancode; }
    constexpr bool operator!=(const KeySource& a, const KeySource& b) { return !(a==b); }

    constexpr bool operator==(const MouseButtonSource& a, const MouseButtonSource& b) { return a.button == b.button; }
    constexpr bool operator!=(const MouseButtonSource& a, const MouseButtonSource& b) { return !(a==b); }

    // NOTe: we use a standard variant here so that all InputSources can be stoed in the same container in the InputHandler. 
    // we use std::variant instead of other type-erasure or polymorphic methods for simplicity and because the set of kinds 
    // is closed and know to the engine at compile time. 
    
    /// @brief Any single raw input that can drive an action. Is a std::variant of all the InputSource types.
    using InputSource = std::variant<KeySource, MouseButtonSource>; 
}