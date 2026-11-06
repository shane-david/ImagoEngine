#include "Imago/Engine/Game.hpp"

#include <cassert>
#include <spdlog/spdlog.h>
#include <optional> // for reading from EngineControl
#include <SFML/System/Clock.hpp> // for calculating dt
#include <SFML/Window/Event.hpp> // for event polling 

namespace Imago::Engine
{
    Game::Game(const GameConfig& config)
        : _config(config), _window(sf::VideoMode({config.width, config.height}), config.title), _sceneManager(_ctx)
        {   
            //TODO integrate assert into the logger
            // make sure the window is a proper size
            assert(config.width > 0 && config.height > 0 && "[Game] Window size must be greater than 0!"); 

            // fill in the context
            _ctx.engine = &_engineControl; 
            _ctx.input = &_inputHandler.state(); 
            //TODO initialize AssetManager
            _ctx.window = &_window; 

            // apply the pacing before the first frame so it starts in the right mode
            apply_pacing(); 
        }

    void Game::apply_pacing() 
    {   
        //TODO integrate asserts/warns into the logger

        FramePacing mode = _config.pacing; // local var so we do not changes the users game settings

        // ensure there is not a limited framerate set to 0
        if (mode == FramePacing::Limited && _config.targetFps == 0) {

            assert(false && "[Game] Limited pacing needs targetFps to be greater than 0!"); 

            // in release it should fall back to VSync so this is recoverable
            spdlog::warn("[Game] Limited pacing needs targetFps to be greater than 0! Falling back to VSync...");
            mode = FramePacing::VSync; 
        }

        // set the SFML pacing for VSynce and framerate limit depending on the settings in _config
        switch (mode) {
        case FramePacing::VSync: 
            _window.setVerticalSyncEnabled(true); 
            _window.setFramerateLimit(0); 
            break; 
        case FramePacing::Limited:
            _window.setVerticalSyncEnabled(false); // there must not be VSynce for a limited frame rate
            _window.setFramerateLimit(_config.targetFps); 
            break;
        case FramePacing::Uncapped:
            _window.setVerticalSyncEnabled(false);
            _window.setFramerateLimit(0); 
            break; 
        }
    }

    void Game::poll_events() 
    {
        // SFML event poll, pollEvent() returns a std::optional<sf::Event>
        while (const std::optional event = _window.pollEvent()) {

            // quit the game for the closed event 
            if (event->is<sf::Event::Closed>()) {
                _engineControl.quit(); 
            }

            // alert the InputHandler about the event
            _inputHandler.handle_event(*event); 
        }
    }

    void Game::process_requests()
    {
        // if there has been a quit requested, set is running to false 
        if (_engineControl.quit_requested()) {
            _isRunning = false; 
            return; 
        }

        // if there is a valid pending switch, forward it to the sceneManager and clear the input edges
        if (const std::optional switchRequest = _engineControl.take_switch_request()) {
            _sceneManager.request_switch(switchRequest->name, switchRequest->persist);
            _inputHandler.clear_edges(); 
            return; 
        }
    }

    void Game::run() 
    {
        // make sure the config actually has a first scene
        assert(!_config.firstScene.empty() && "[Game] There is not first scene set in GameConfig!"); 
        if (_config.firstScene.empty()) {
            spdlog::warn("[Game] there is no firstScene in GameConfig! Cannot start game...");
            return; 
        }

        // start the first scene through the SceneManager, immediately apply the switch since nothing is yet running
        _sceneManager.request_switch(_config.firstScene, false); 
        _sceneManager.apply_pending_switch(); 

        // get rid of any input that could have been happening before the first scene was initialized 
        _inputHandler.clear_edges(); 

        // create the SFML clock so that we can set the dt in the isRunning loop
        // intentionally created after the first scene so it does not contain the first scene's load time
        sf::Clock clock; 

        // set isRunning and start running the game, this loop runs every frame 
        _isRunning = true; 
        while (_isRunning) {

            // --- per frame logic ---

            // NOTE: here SFML does most of the heavy lifting. clock.restart() returns an sf::Time
            // that holds how much time has passed since the clock last started and resets the clock to 0
            // .asSeconds() converts this from sf::Time to a float that can be passed down through the pipeline

            // set the dt since last frame
            const float dt = clock.restart().asSeconds(); 

            // read close event and TODO: input
            poll_events(); 

            // update the current scene and run all systems 
            _sceneManager.update(dt); 

            // --- post frame logic --- 

            // check for and process scene switch and quit events
            process_requests(); 

            // quit immediately after if isRunning is false 
            if (!_isRunning) break; 

            // apply any pending switches
            _sceneManager.apply_pending_switch(); 

            // --- rendering ---
            _window.clear(); 
            //TODO rendering system
            _window.display(); 
        }
    }

    // --- forwards ---

    ActionId Game::register_action(const std::string& name)
    {
        return _inputHandler.register_action(name); 
    }

    void Game::add_input(const std::string& name, sf::Keyboard::Scancode scancode) 
    {
        _inputHandler.add_input(name, scancode); 
    }

    void Game::add_input(const std::string& name, sf::Mouse::Button button) 
    {
        _inputHandler.add_input(name, button); 
    }
}