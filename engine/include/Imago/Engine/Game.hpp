/**
 * @file Game.hpp
 * @author Shane David
 * @brief Defines Game, the owner of the window and persistent game objects that run the entire game loop. 
 */

#pragma once

#include <string> 
#include <SFML/Graphics/RenderWindow.hpp> // for the window lifetime and rendering

#include "Imago/Engine/GameConfig.hpp"
#include "Imago/Engine/EngineControl.hpp"
#include "Imago/Engine/SceneContext.hpp"
#include "Imago/Engine/SceneManager.hpp"

namespace Imago::Engine 
{
    /**
     * @brief The owner of the engine. Creates the window and every persistent engine object, filling in SceneContext 
     * with those objects before passing it down to the Scenes. 
     * 
     * Game is infastructure, it does not execute any logic. It measures delta time, polls window events, updates 
     * the SceneManager, applies requests made through EngineControl, and handles the window. Requess are applied
     * at the end of each frame, after the update, so the Scene is never destroyed while it is still running. 
     * 
     * Scenes and systems never reference Game directly. They only see the SceneContext it builds and talk to the
     * Game through ctx.engine. 
     */
    class Game {
    private:
        
        // NOTE: member order matters in this class. Members are destroyed in reverse in C++, so the SceneManager
        // needs to unload its Scenes while the window, EngineControl, and context are all still alive. The SceneManager
        // holds a const SceneContext& so _ctx must be declared before it. 

        GameConfig _config; ///< settings the Game was constructed with 
        bool _isRunning = false; ///< whether or not the main game loop should be running 
        sf::RenderWindow _window; ///< the SFML game window
        //TODO input map and handler 
        //TODO AssetManager
        EngineControl _engineControl; ///< records requests for quitting and scene changes from systems
        SceneContext _ctx; ///< root context that is filled in with persistent Engine objects that every Scene copies
        SceneManager _sceneManager; ///< owns and switches Scenes 

        /**
         * @brief Applies excactly one pacing mode from the config (VSync by default)
         * 
         * - VSync turns frame limit off. The window waits for the monitor's refresh, so the 
         *   frame rate matches the display. It prevents screen tearing and uses the least CPU and GPU
         *   but the fram rate depends on the player's monitor.
         * 
         * - Limited turns VSync off and the frame rate limit is set the the targetFps in GameSettings. SFML sleeps at 
         *   the end of each frame to hold that framerate regardless of the monitor. The frame rate is stable and predictable 
         *   but tearing is possible and the limiter is not exact. 
         * 
         * - Uncapped turns VSync and the frame rate limit off. The loop runs as fast as the machine allows which makes 
         *   the dt very small but CPU and GPU use very high. 
         */
        void apply_pacing(); 

        /**
         * @brief Drains the SFML window event queue. 
         * 
         * Only the close event is handled for now. 
         */
        void poll_events(); 

        /**
         * @brief Reads EngineControls current requests. 
         * 
         *  - Sets _isRunning to false on quit.
         * 
         *  - Forwards a pending switch to the SceneManager on switch_scene
         */
        void process_requests(); 

    public:
        
        /**
         * @brief Construct a new Game object, building a new window based on GameSettings and filling in the SceneContext with its persistent engine objects. 
         * @param config Settings for the window, pacing, and first Scene 
         */
        explicit Game(const GameConfig& config); 

        // default destructor
        ~Game() = default; 

        // explicitly disallow copy and move behavior
        Game(const Game&) = delete;
        Game& operator=(const Game&) = delete;
        Game(Game&&) = delete;
        Game& operator=(Game&&) = delete; 

        /**
         * @brief Registers a Scene type under a name. Simply forwards to its SceneManager. 
         * @tparam T Scene type, must satisfy SceneManager::register_scene's requiremenets.
         * @param name Name of the Scene. Used to switch between Scenes and instantiate the first scene. 
         */
        template <typename T>
        void register_scene(const std::string& name) { _sceneManager.register_scene<T>(name); }

        /**
         * @brief Starts config.firstScene and runs the main loop until the game quits. 
         * 
         */
        void run(); 
    }; 
}