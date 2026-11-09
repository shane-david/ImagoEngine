#include "Imago/Engine/Game.hpp"

// NOTE: we make this preprocessor directive so that SDL2 does not hijack main 
#define SDL_MAIN_HANDLED
#include <SDL.h> 

#include <cassert>
#include <spdlog/spdlog.h>
#include <cmath> // for std::fabs
#include <algorithm> // for std::clamp
#include <optional> // for reading from EngineControl
#include <SFML/System/Clock.hpp> // for calculating dt
#include <SFML/Window/Event.hpp> // for event polling 

namespace Imago::Engine
{   
    /// @brief internal because this cpp file should be the only one to ever convert from SDL2 to Imago enums, everwhere else exclusively deals with Imago enums
    namespace
    {   
        /// @brief helper struct to return both the positive and negative GamepadAxis when a SDL2 axis is passed in 
        struct AxisDirections {
            GamepadAxis negative; // unknown for triggers 
            GamepadAxis positive; 
        }; 

        /// @brief  Converts an SDL_GameControllerButton enum to Imago's GamepadButton enum 
        GamepadButton from_sdl(SDL_GameControllerButton button) 
        {
            switch (button) {

                // face buttons 
                case SDL_CONTROLLER_BUTTON_X: return GamepadButton::FaceLeft;
                case SDL_CONTROLLER_BUTTON_B: return GamepadButton::FaceRight; 
                case SDL_CONTROLLER_BUTTON_Y: return GamepadButton::FaceUp; 
                case SDL_CONTROLLER_BUTTON_A: return GamepadButton::FaceDown; 

                // menu buttons
                case SDL_CONTROLLER_BUTTON_BACK: return GamepadButton::Back; 
                case SDL_CONTROLLER_BUTTON_GUIDE: return GamepadButton::Guide;
                case SDL_CONTROLLER_BUTTON_START: return GamepadButton::Start; 

                // stick presses
                case SDL_CONTROLLER_BUTTON_LEFTSTICK: return GamepadButton::LeftStick;
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return GamepadButton::RightStick; 

                // shoulder buttons
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return GamepadButton::LeftShoulder;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return GamepadButton::RightShoulder; 

                // d pad buttons
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return GamepadButton::DPadLeft; 
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return GamepadButton::DPadRight; 
                case SDL_CONTROLLER_BUTTON_DPAD_UP: return GamepadButton::DPadUp; 
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return GamepadButton::DPadDown; 

                // unknown 
                default: return GamepadButton::Unknown; 
            }
        }

        /// @brief Converts an SDL_GameConterollerAxis enum to an AxisDirections struct containing both the positive and negative Imago GamepadAxis enums 
        AxisDirections from_sdl(SDL_GameControllerAxis axis) 
        {
            // NOTE: SDL's Y  axis is positive downward matching SFML, so negative is up
            switch (axis) {

                // left stick 
                case SDL_CONTROLLER_AXIS_LEFTX: return { GamepadAxis::LeftStickLeft, GamepadAxis::LeftStickRight }; 
                case SDL_CONTROLLER_AXIS_LEFTY: return { GamepadAxis::LeftStickUp, GamepadAxis::LeftStickDown }; 

                // right stick 
                case SDL_CONTROLLER_AXIS_RIGHTX: return { GamepadAxis::RightStickLeft, GamepadAxis::RightStickRight };
                case SDL_CONTROLLER_AXIS_RIGHTY: return { GamepadAxis::RightStickUp, GamepadAxis::RightStickDown }; 

                // NOTE: triggers can only be positive so they pass in unknown for the negative part of AxisDirections 
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return { GamepadAxis::Unknown, GamepadAxis::LeftTrigger };
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return { GamepadAxis::Unknown, GamepadAxis::RightTrigger };

                // unknown
                default: return { GamepadAxis::Unknown, GamepadAxis::Unknown }; 
            }
        }
    }

    // --- config --- 

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

            // NOTE: SDL_INIT_GAMECONTROLLER implies the joystick subsytem, which implies the event queue, so this one flag gets everything 
            // poll_gamepad_events needs.
            if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
                //TODO integrate warn into loger
                spdlog::warn("[Game] gamepad support unavailable: {}", SDL_GetError()); 
            } else {
                _gamepadReady = true; 
                adopt_any_controller(); 
            }
        }

    Game::~Game()
    {
        // NOTE: it is important here that closing the controller happens before quiiting SDL
        close_controller();
        SDL_Quit(); 
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

    /// --- event polling --- 

    void Game::poll_events() 
    {
        // SFML event poll, pollEvent() returns a std::optional<sf::Event>
        while (const std::optional event = _window.pollEvent()) {

            // quit the game for the closed event 
            if (event->is<sf::Event::Closed>()) {
                _engineControl.quit(); 
            }

            // set the _hasFocus bool accordingly for focus events
            if (event->is<sf::Event::FocusLost>()) {
                _hasFocus = false; 
            } else if (event->is<sf::Event::FocusGained>()) {
                _hasFocus = true; 
            }

            // alert the InputHandler about the event
            _inputHandler.handle_event(*event); 
        }
    }

    void Game::poll_gamepad_events() 
    {
        // if there was never a gamepad do not even run the event loop
        if (!_gamepadReady) return; 

        // SDL event loop 
        SDL_Event event; 
        while (SDL_PollEvent(&event)) {
            switch (event.type) {

                // if a controller was added try ot open it with the cdevice.which index 
                case SDL_CONTROLLERDEVICEADDED:
                    try_open_controller(event.cdevice.which); 
                    break;
                
                // if a controller was removed close it and try to adopt a new one, only if its id matches the current id (we do not care about unopened controller disconnects)
                case SDL_CONTROLLERDEVICEREMOVED: 
                    if (event.cdevice.which == _controllerId) {
                        close_controller();
                        adopt_any_controller(); 
                    }
                    break;
                
                // if a controller button down or button up event happens notify the InputHandler
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP:
                {   
                    // does not matter if we do not have focus or if the event is from an unopened controller
                    if (!_hasFocus) break;
                    if (event.cbutton.which != _controllerId) break; 

                    // construct the GamepadButton using the from_sdl helper
                    const GamepadButton button = from_sdl(static_cast<SDL_GameControllerButton>(event.cbutton.button)); 

                    // notify the InputHandler about the event, passing in whether or not it was a press 
                    _inputHandler.handle_gamepad_button(button, event.cbutton.state == SDL_PRESSED); 
                    break; 
                }

                // if there was controller axis motion notify the InputHandler
                case SDL_CONTROLLERAXISMOTION:
                {
                    // again, does not matter if there is no focus or if the event is from an unopened controller
                    if (!_hasFocus) break;
                    if (event.caxis.which != _controllerId) break; 

                    // construct the AxisDirections using from_sdl helper
                    const auto axis = static_cast<SDL_GameControllerAxis>(event.caxis.axis); 
                    const AxisDirections directions = from_sdl(axis); 

                    // in SDL sticks report from -32768-32768, triggers from 0-32767. Normalize to 0-1 clamping because the negative is one larger than the positive end
                    const Sint16 raw = event.caxis.value; 
                    const float magnitude = std::clamp(std::fabs(static_cast<float>(raw)) / 32767.0f, 0.0f, 1.0f);

                    // NOTE: here we notify the InputHandler about BOTH directions even though one recieves 0. This is to essentially notify a release, 
                    // if we do not, when the stick swings across center in one event, nothing else will tell the direction that it was released. 

                    // NOTE: the ternary operator here is essentially saying if the raw value is in the corresponding direction, pass in the magnitude, otherwise pass in 0

                    // do not notify for an unknown axis for either right or left
                    if (directions.negative != GamepadAxis::Unknown) {
                        _inputHandler.handle_gamepad_axis(directions.negative, raw < 0 ? magnitude : 0.0f); 
                    } 
                    if (directions.positive != GamepadAxis::Unknown) {
                        _inputHandler.handle_gamepad_axis(directions.positive, raw > 0 ? magnitude : 0.0f); 
                    }

                    break; 
                }

                default:
                    break; 
            }
        }
    }

    /// -- private helpers --- 

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

    // --- controller helpers ---

    bool Game::try_open_controller(int index)
    {
        // TODO integrate multiple controllers
        // for now we only track one controller, so a second one is ignored
        if (_controller != nullptr) return false; 

        // a joystick that SDL has no controller mapping for is something that we cannot use
        if (!SDL_IsGameController(index)) return false; 

        // try to open the controller at that index and log a warning if it failed
        SDL_GameController* controller = SDL_GameControllerOpen(index); 
        if (controller == nullptr) {
            //TODO integrate into logger
            spdlog::warn("[Game] could not open controller at index {}: {}", index, SDL_GetError()); 
            return false; 
        }

        // NOTE: the device index is only valid at this instant, every later event carries an INSTANCE ID instead, so conver once here and never store the index

        // otherwise set the controller and its id 
        _controller = controller;
        _controllerId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)); 

        // TODO initial state synce on connect and focus regain

        // NOTE: SDL_GameControllerName can return null, and formatting a null char* is undefined so we check it first

        // print out that the controller was connected and return that status 
        const char* name = SDL_GameControllerName(controller); 
        spdlog::info("[Game] controller connected: {}", name ? name : "unknown"); 

        return true; 
    }

    void Game::close_controller()
    {
        // cannot close a controller if one is not open
        if (_controller == nullptr) return; 

        // close it in SDL then change the members to represent that 
        SDL_GameControllerClose(_controller); 
        _controller = nullptr;
        _controllerId = -1; 

        // release the inputs so that nothing gets stuck down forever on an disconnect
        _inputHandler.release_gamepad_inputs(); 
    }

    void Game::adopt_any_controller()
    {   
        // go through all connected joystick and try to open them until one opens 
        const int count = SDL_NumJoysticks(); 
        for (int i = 0; i < count; i++) {
            if (try_open_controller(i)) return; 
        }
    }
    

    // --- game loop --- 

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

            // poll events
            poll_events(); 
            poll_gamepad_events(); 

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

    void Game::add_input(const std::string& name, GamepadButton button) 
    {
        _inputHandler.add_input(name, button); 
    }

    void Game::add_input(const std::string& name, GamepadAxis axis, float deadZone, float threshold) 
    {
        _inputHandler.add_input(name, axis, deadZone, threshold); 
    }
}