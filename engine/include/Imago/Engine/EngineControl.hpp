/**
 * @file EngineControl.hpp
 * @author Shane David
 * @brief Define EngineControl which is the object that the user calls quit and scene change methdos through
 */

#pragma once

#include <string> // for scene names in the switch_scene method
#include <optional> // for returning values in the take request methods

namespace Imago::Engine 
{
    /**
     * @brief The user's only way to talk to the Game from inside a system or Scene load/unload. 
     * 
     * Every method only records a request and returns immediatley. Game then consuems and applies the request at the end
     * of each frame, so the rest of the current fram still runs after a call. 
     */
    class EngineControl {
    private:
        
        // game needs to be a friend class so that it can read the requests 
        friend class Game; 
        friend struct EngineControlTestAccess; //TODO: this is just for testing and should be removed in the release build 

        /// @brief A recorded Scene switch request 
        struct SwitchRequest {
            std::string name; ///< name of the Scene to switch to
            bool persist; ///< whether the current Scene should persist through the switch
        }; 

        std::optional<SwitchRequest> _pendingSwitch; ///< the latest switch request, empty if none
        bool _quitRequested = false; ///< true once quit() has been called, tells Game to quit

        /**
         * @brief Hands the pending switch to the caller and clears it. Called by Game to determine if there ought to be a switch. 
         * @return The pending request or an empty optional if there was none. 
         */
        std::optional<SwitchRequest> take_switch_request();  

        /**
         * @brief Whether or not a quit has been requested. Called by Game to see if the user requested a quit within a System. 
         */
        bool quit_requested() const; 

    public:

        /**
         * @brief Construct a new Engine Control object
         */
        EngineControl() = default; 

        // explicity disallow copy and move behavior 
        EngineControl(const EngineControl&) = delete;
        EngineControl& operator=(const EngineControl&) = delete;
        EngineControl(EngineControl&&) = delete; 
        EngineControl& operator=(EngineControl&&) = delete; 

        /**
         * @brief Requests a switch to a registered Scene. Called by the user within a system through the engine field in SceneContext. 
         * If called more than once in a frame, the last call wins. 
         * 
         * @param name 
         * @param persist 
         */
        void switch_scene(const std::string& name, bool persist = false); 

        /**
         * @brief Requests that the game ends. Called by the user within a system through the engine field in SceneContext. 
         * The current frame finishes and any pending scene change is skipped. 
         * 
         */
        void quit(); 
    }; 
}