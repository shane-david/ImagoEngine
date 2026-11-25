/**
 * @file EngineTestAccess.hpp
 * @brief Test-only access to the private parts of EngineControl and InputState, through the
 * friend declarations those headers provide.
 *
 * Every member is an inline static function defined inside its class, so this header can be
 * included from any number of test files without breaking the one-definition rule.
 */

#pragma once

#include <optional>
#include <string>

#include "Imago/Engine/EngineControl.hpp"
#include "Imago/Engine/InputState.hpp"

namespace Imago::Engine
{
    /// @brief Reads the requests EngineControl records (friend struct in EngineControl.hpp).
    struct EngineControlTestAccess {
        static bool has_pending(const EngineControl& e) { return e._pendingSwitch.has_value(); }
        static auto take(EngineControl& e) { return e.take_switch_request(); }
        static bool quit_requested(const EngineControl& e) { return e.quit_requested(); }
    };

    /// @brief Writes InputState directly and drives its phase hooks (friend class in InputState.hpp).
    class InputStateTestAccess {
    public:
        static ActionId add_action(InputState& s, const std::string& name)
        {
            return s.add_action(name);
        }

        /// @brief Sets strength and derives held as strength > 0.
        static void set_strength(InputState& s, ActionId action, float value)
        {
            s.set_state(action, value, value > 0.0f);
        }

        static void clear_edges(InputState& s) { s.clear_edges(); }
        static void begin_frame_phase(InputState& s) { s.begin_frame_phase(); }
        static void end_frame_phase(InputState& s) { s.end_frame_phase(); }
        static void begin_fixed_step(InputState& s) { s.begin_fixed_step(); }
        static void end_fixed_step(InputState& s) { s.end_fixed_step(); }
    };
}
