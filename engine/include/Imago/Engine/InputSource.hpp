/**
 * @file InputSource.hpp
 * @author Shane David
 * @brief Defines the raw inputs that drive an action
 */

#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <variant> 
#include <cstdint> // for uint8_t for the enums 

namespace Imago::Engine 
{
    // --- Imago Input Enum definitions --- 

    enum class GamepadButton : uint8_t
    {
        FaceLeft, // X in SDL2
        FaceRight, // B in SDL2
        FaceUp, // Y in SDL2
        FaceDown, // A in SDL2
        Back, Guide, Start,
        LeftStick, RightStick, // the presses not movement
        LeftShoulder, RightShoulder,
        DPadLeft, DPadRight, DPadUp, DPadDown,
        Unknown
    }; 

    enum class GamepadAxis : uint8_t
    {
        LeftStickLeft,
        LeftStickRight,
        LeftStickUp,
        LeftStickDown,
        RightStickLeft,
        RightStickRight,
        RightStickUp,
        RightStickDown,
        LeftTrigger,
        RightTrigger,
        Unknown
    }; 

    // --- input sources and their variant overloads --- 

    /// @brief A keybaord key that drives an action. Reads as 0 or 1. (pressed or not pressed)
    struct KeySource { sf::Keyboard::Scancode scancode; }; 

    /// @brief A mouse button that drives an action. Reads as 0 or 1. (pressed or not pressed)
    struct MouseButtonSource { sf::Mouse::Button button; }; 

    /// @brief A gamepad button that drives an action. Reads as 0 or 1. (pressed or not pressed)
    struct GamepadButtonSource { GamepadButton button; };

    /**
     * @brief A gamepad axis motion that drives an action. Reads as a strength rescaled to a dead zone 
     * and scaled to a magnitude between 0-1.
     * 
     * Also reads as held when the axis is pressed past a threshold. 
     */
    struct GamepadAxisSource { GamepadAxis axis; }; 

    // NOTE: std::variant's == operator is used by the InputHandler to find matching inputs. It only exists if every alternative 
    // of the variant has its own == operator. The variant compares the held type first, so a KeySource never equals a MouseButtonSource.

    constexpr bool operator==(const KeySource& a, const KeySource& b) { return a.scancode == b.scancode; }
    constexpr bool operator!=(const KeySource& a, const KeySource& b) { return !(a==b); }

    constexpr bool operator==(const MouseButtonSource& a, const MouseButtonSource& b) { return a.button == b.button; }
    constexpr bool operator!=(const MouseButtonSource& a, const MouseButtonSource& b) { return !(a==b); }

    constexpr bool operator==(const GamepadButtonSource& a, const GamepadButtonSource& b) { return a.button == b.button; }
    constexpr bool operator!=(const GamepadButtonSource& a, const GamepadButtonSource& b) { return !(a==b); }

    constexpr bool operator==(const GamepadAxisSource& a, const GamepadAxisSource& b) { return a.axis == b.axis; }
    constexpr bool operator!=(const GamepadAxisSource& a, const GamepadAxisSource& b) { return !(a==b); }

    // NOTe: we use a standard variant here so that all InputSources can be stoed in the same container in the InputHandler. 
    // we use std::variant instead of other type-erasure or polymorphic methods for simplicity and because the set of kinds 
    // is closed and know to the engine at compile time. 
    
    /// @brief Any single raw input that can drive an action. Is a std::variant of all the InputSource types.
    using InputSource = std::variant<KeySource, MouseButtonSource, GamepadButtonSource, GamepadAxisSource>; 
}