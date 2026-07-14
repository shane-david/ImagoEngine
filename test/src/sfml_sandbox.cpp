// =============================================================================
// sfml_learning.cpp
// ImagoEngine Phase 1 Sandbox
//
// GOALS COVERED IN THIS FILE:
//   [x] SFML window creation
//   [x] Load texture from disk, render sprite
//   [x] SFML keyboard input (WASD) with delta time movement
//   [x] SFML audio load and play a sound effect
//   [x] SFML sf::View camera pan and zoom with arrow keys
//   [x] Dear ImGui + imgui-sfml dockspace rendering alongside SFML viewport
//   [x] SDL2 GameController open controller, read axes, buttons, triggers
//   [x] Log controller input values to console
//   [x] Control sprite with controller left stick
// =============================================================================

// -----------------------------------------------------------------------------
// INCLUDES
// Each header gives us access to a different library or system feature.
// -----------------------------------------------------------------------------

// SFML Graphics RenderWindow, Sprite, Texture, View, Clock, Color, etc.
// Including Graphics.hpp automatically includes Window.hpp and System.hpp too,
// so we don't need to include those separately.
#include <SFML/Graphics.hpp>

// SFML Audio SoundBuffer, Sound, Music
// This is a separate module that needs to be included (and linked) on its own.
#include <SFML/Audio.hpp>

// Dear ImGui the core ImGui library.
// This gives us ImGui::Begin(), ImGui::Button(), ImGui::Text(), etc.
#include "imgui.h"

// imgui-SFML the backend that connects ImGui to your SFML window.
// This gives us ImGui::SFML::Init(), Update(), Render(), Shutdown().
#include "imgui-SFML.h"

// SDL2 we only use this for the GameController API.
// SDL2 and SFML coexist fine in the same program they manage different things.
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

// spdlog 
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

// Standard library for std::cout, std::cerr, std::abs
#include <iostream>
#include <cmath>
#include <vector>

// =============================================================================
// MAIN
// =============================================================================
int main()
{
    // =========================================================================
    // SECTION 1: SDL2 INITIALIZATION
    // SDL2 must be initialized before the SFML window is created.
    // We only pass SDL_INIT_GAMECONTROLLER because that's the only SDL2
    // subsystem we're using we don't need SDL's video or audio.
    // =========================================================================
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0)
    {
        // SDL_GetError() returns a human-readable string describing the error.
        std::cerr << "ERROR: SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // =========================================================================
    // Section 1.6: LOGGER SETUP
    // Set up before the window so every failur after this point can 
    // us the logger instead of std::cerr
    // =========================================================================
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::debug); 

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/engine.log", true); 
    fileSink->set_level(spdlog::level::debug); 

    std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink}; 
    auto log = std::make_shared<spdlog::logger>("Engine", sinks.begin(), sinks.end()); 
    log->set_level(spdlog::level::trace); 
    spdlog::register_logger(log);
    spdlog::set_default_logger(log); 

    log->info("--- ImagoEngine Sandbox Starting ---"); 
    
    // =========================================================================
    // SECTION 2: SFML WINDOW
    // sf::RenderWindow is the main SFML class it's both a window and a
    // 2D drawing surface. sf::VideoMode takes the width and height as a
    // Vector2u (unsigned int vector), which is the SFML 3.0 style.
    // =========================================================================
    sf::RenderWindow window(sf::VideoMode({1280, 720}), "ImagoEngine Sandbox");

    // Limit the frame rate so the loop doesn't run at thousands of FPS.
    // This also helps keep delta time values stable.
    window.setFramerateLimit(60);

    // =========================================================================
    // SECTION 3: IMGUI-SFML INITIALIZATION
    // ImGui::SFML::Init links ImGui to our SFML window.
    // It sets up the font atlas, input handling bridge, and OpenGL context.
    // This MUST be called after the window is created and before the game loop.
    // =========================================================================
    if (!ImGui::SFML::Init(window))
    {
        log->critical("ImGui::SFML::Init failed.");
        SDL_Quit();
        return -1;
    }

    // Enable the docking feature in ImGui.
    // ImGui::GetIO() returns a reference to ImGui's global configuration struct.
    // ConfigFlags is a bitmask we OR in the docking flag to enable it
    // without clearing any other flags that might already be set.
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // =========================================================================
    // SECTION 4: SFML AUDIO SOUND EFFECT
    // Audio in SFML works like graphics:
    //   sf::SoundBuffer = sf::Texture  (holds the raw data, lives on the CPU)
    //   sf::Sound       = sf::Sprite   (lightweight playback handle)
    //
    // The SoundBuffer MUST outlive the Sound that references it.
    // Both are declared here in main() scope so they live for the entire program.
    //
    // SUPPORTED FORMATS: WAV, OGG, FLAC. MP3 is decode-only.
    // Place your audio file in the assets/sounds/ folder next to your exe,
    // or adjust the path below to match where your file actually is.
    // =========================================================================
    sf::SoundBuffer hitBuffer;
    if (!hitBuffer.loadFromFile("assets/hit.wav"))
    {
        log->error("Could not load sound at: {}", "assets/hit.wav"); 
        // We don't return here missing audio is non-fatal for learning purposes.
        // In a real engine this would be a hard failure.
    }

    // Create the sound and attach the buffer to it.
    // sound.play() fires the sound asynchronously it runs on a background
    // thread and doesn't block the game loop.
    sf::Sound hitSound(hitBuffer);
    hitSound.setVolume(75.f); // volume range is 0 (mute) to 100 (full)

    // =========================================================================
    // SECTION 5: SFML GRAPHICS TEXTURE AND SPRITE
    // sf::Texture loads the image data from disk onto the GPU.
    // sf::Sprite is a lightweight drawable that references the texture.
    //
    // The texture MUST outlive the sprite if the texture is destroyed,
    // the sprite will draw garbage or crash.
    // =========================================================================
    sf::Texture playerTexture;
    if (!playerTexture.loadFromFile("assets/player.png"))
    {
        log->error("Could not load texture at: {}", "assets/player.png"); 
        ImGui::SFML::Shutdown();
        SDL_Quit();
        return -1;
    }

    // Construct the sprite with the texture.
    // In SFML 3.0, sf::Sprite requires a texture at construction time
    // there is no default constructor like in SFML 2.x.
    sf::Sprite playerSprite(playerTexture);

    // Set the origin to the center of the sprite.
    // The origin is the pivot point for position, rotation, and scale.
    // getLocalBounds() returns the bounding box in local coordinates (before
    // any transforms), so dividing its size by 2 gives us the center.
    // This means setPosition() will place the CENTER of the sprite at that point,
    // not the top-left corner.
    sf::FloatRect spriteBounds = playerSprite.getLocalBounds();
    playerSprite.setOrigin({spriteBounds.size.x / 2.f, spriteBounds.size.y / 2.f});

    // Place the sprite in the center of the window to start.
    playerSprite.setPosition({640.f, 360.f});

    // =========================================================================
    // SECTION 6: SFML VIEW (2D CAMERA)
    // sf::View defines what portion of the 2D world is visible in the window.
    // Think of it as a camera that can pan, zoom, and rotate over the world.
    //
    // window.getDefaultView() returns a view that exactly matches the window
    // size a good starting point.
    //
    // After any modification to the view, you MUST call window.setView(view)
    // to apply the changes. The window keeps a copy of the view, not a reference.
    // =========================================================================
    sf::View camera = window.getDefaultView();

    // How fast the camera pans in pixels per second.
    const float CAMERA_SPEED = 400.f;

    // =========================================================================
    // SECTION 7: SDL2 CONTROLLER OPEN FIRST CONNECTED CONTROLLER
    // SDL_NumJoysticks() returns how many joysticks/controllers are connected.
    // SDL_IsGameController() checks if a joystick supports the GameController
    // API (which gives us named buttons/axes instead of raw indices).
    // SDL_GameControllerOpen() opens the controller for use and returns a
    // pointer to it. We hold onto this pointer for the rest of the program.
    // =========================================================================
    SDL_GameController* controller = nullptr;

    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            controller = SDL_GameControllerOpen(i);
            if (controller != nullptr)
            {   
                log->info("Controller connected: {}", SDL_GameControllerName(controller)); 
                break; // stop after finding the first valid controller
            }
        }
    }

    if (controller == nullptr)
    {
        log->info("No controller found keyboard/mouse only."); 
    }

    // Dead zone for the analog sticks.
    // Stick axes return values from -32768 to 32767.
    // Even at rest, sticks return small non-zero values due to hardware noise.
    // We treat anything within this range as zero to avoid drift.
    const Sint16 DEAD_ZONE = 8000;

    // =========================================================================
    // SECTION 8: GAME STATE VARIABLES
    // =========================================================================

    // Player velocity  reset to zero every frame, then rebuilt from input.
    sf::Vector2f playerVelocity = {0.f, 0.f};

    // How fast the player moves in pixels per second.
    const float PLAYER_SPEED = 300.f;

    // Volume variable that ImGui can modify via a slider widget.
    float soundVolume = 75.f;

    // Whether to show the ImGui demo window (useful for exploring widgets).
    bool showDemoWindow = false;

    // sf::Clock tracks elapsed time. clock.restart() returns how much time
    // has passed since the last restart and resets the timer.
    // We use this to calculate delta time (time between frames).
    sf::Clock deltaClock;

    // =========================================================================
    // SECTION 9: MAIN LOOP
    // The main loop runs once per frame, roughly 60 times per second here.
    // Each iteration: process events → update state → render → display.
    // =========================================================================
    while (window.isOpen())
    {   

        // =====================================================================
        // STEP 9A: SFML EVENT LOOP
        // window.pollEvent() checks if there are any pending events (key press,
        // window close, mouse move, etc.) and returns the next one as a
        // std::optional<sf::Event>. It returns an empty optional when there
        // are no more events, which exits the while loop.
        //
        // IMPORTANT: ImGui::SFML::ProcessEvent must be called for EVERY event
        // BEFORE you handle it yourself. This lets ImGui consume input when
        // its panels are focused (e.g. typing in a text box shouldn't also
        // move your player).
        // =====================================================================
        while (const std::optional event = window.pollEvent())
        {
            // Give ImGui first access to every event.
            ImGui::SFML::ProcessEvent(window, *event);
            // The * dereferences the std::optional to get the sf::Event inside.

            // Check if the user clicked the X button to close the window.
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            // getIf<T>() checks if the event is of type T.
            // If it is, it returns a pointer to the event data. If not, nullptr.
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                // Close window on Escape key.
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
                {
                    window.close();
                }

                // Play sound effect on Spacebar.
                // This is in the event loop (not polled) because we want it
                // to fire once per key press, not every frame while held.
                if (keyPressed->scancode == sf::Keyboard::Scancode::Space)
                {
                    hitSound.play();
                    log->info("Sound played!");
                }
            }
        }

        // =====================================================================
        // STEP 9B: SDL2 EVENT LOOP (for controller hot-plugging)
        // SDL has its own separate event queue from SFML.
        // We poll it here to detect controllers being connected/disconnected
        // while the program is running. If you only need to detect controllers
        // at startup, this block can be skipped but it's good practice.
        // =====================================================================
        SDL_Event sdlEvent;
        while (SDL_PollEvent(&sdlEvent))
        {
            if (sdlEvent.type == SDL_CONTROLLERDEVICEADDED)
            {
                // A new controller was plugged in while the game is running.
                if (controller == nullptr) // only grab if we don't have one yet
                {
                    controller = SDL_GameControllerOpen(sdlEvent.cdevice.which);
                    log->info("Controller connected: {}", SDL_GameControllerName(controller)); 
                }
            }

            if (sdlEvent.type == SDL_CONTROLLERDEVICEREMOVED)
            {
                // The controller was unplugged.
                if (controller != nullptr)
                {
                    SDL_GameControllerClose(controller);
                    controller = nullptr;
                    log->info("Controller disconnected."); 
                }
            }
        }

        // =====================================================================
        // STEP 9C: IMGUI UPDATE
        // ImGui::SFML::Update must be called once per frame before any ImGui
        // widget calls (ImGui::Begin, ImGui::Button, etc.).
        // It internally calls ImGui::NewFrame() which starts a fresh ImGui frame.
        // We pass deltaClock.restart() so ImGui knows how much time has passed
        // (it uses this for animations and input repeat timing).
        // =====================================================================
        sf::Time dt = deltaClock.restart();
        ImGui::SFML::Update(window, dt);

        // Convert delta time to seconds as a float for movement calculations.
        // At 60 FPS this is roughly 0.0166f seconds per frame.
        float dtSeconds = dt.asSeconds();
        log->trace("Fram start - dt: {:4f}s", dtSeconds); 

        // =====================================================================
        // STEP 9D: CAMERA INPUT (Arrow Keys pan and zoom)
        // sf::Keyboard::isKeyPressed() returns true every frame the key is held.
        // This is different from the KeyPressed EVENT which fires only once.
        // We use polling here because panning is a continuous held-key action.
        //
        // After modifying the view, we must call window.setView(camera) to
        // apply it the window stores a COPY of the view, not a reference.
        // =====================================================================
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Up))
            camera.move({0.f, -CAMERA_SPEED * dtSeconds});

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Down))
            camera.move({0.f, CAMERA_SPEED * dtSeconds});

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Left))
            camera.move({-CAMERA_SPEED * dtSeconds, 0.f});

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Right))
            camera.move({CAMERA_SPEED * dtSeconds, 0.f});

        // Zoom in/out with Z and X keys.
        // view.zoom() multiplies the current view size by a factor.
        //   factor < 1.0 → smaller view area = zoomed IN
        //   factor > 1.0 → larger view area = zoomed OUT
        // We clamp it slightly so it doesn't zoom infinitely in either direction.
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Z))
            camera.zoom(1.f - (1.5f * dtSeconds)); // zoom in

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::X))
            camera.zoom(1.f + (1.5f * dtSeconds)); // zoom out

        // Apply the (potentially modified) camera to the window.
        // Everything drawn after this call will be affected by the view.
        window.setView(camera);

        // =====================================================================
        // STEP 9E: PLAYER KEYBOARD INPUT (WASD)
        // Reset velocity to zero at the start of each frame.
        // Then check which keys are held and build the velocity vector.
        // Multiplying by PLAYER_SPEED gives us pixels-per-second.
        // We apply delta time during the move step, not here.
        // =====================================================================
        playerVelocity = {0.f, 0.f};

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::W))
            playerVelocity.y -= PLAYER_SPEED;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::S))
            playerVelocity.y += PLAYER_SPEED;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::A))
            playerVelocity.x -= PLAYER_SPEED;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::D))
            playerVelocity.x += PLAYER_SPEED;

        // =====================================================================
        // STEP 9F: SDL2 CONTROLLER INPUT
        // SDL_GameControllerGetAxis() reads a named analog axis.
        // SDL_GameControllerGetButton() reads a named button (1=pressed, 0=not).
        //
        // Stick axis range: -32768 to 32767 (Sint16)
        // Trigger axis range: 0 to 32767
        //
        // We normalize sticks to -1.0..1.0 by dividing by 32767.f.
        // We apply a dead zone to prevent drift when the stick is at rest.
        // =====================================================================
        if (controller != nullptr)
        {
            // --- Read left stick axes ---
            Sint16 leftX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            Sint16 leftY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

            // --- Read right stick axes (for reference / future use) ---
            Sint16 rightX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
            Sint16 rightY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

            // --- Read triggers ---
            // Triggers go from 0 (not pressed) to 32767 (fully pressed).
            Sint16 leftTrigger  = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            Sint16 rightTrigger = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

            // --- Read face buttons ---
            // Returns 1 if the button is currently pressed, 0 if not.
            Uint8 buttonA = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
            Uint8 buttonB = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);

            // --- Apply dead zone to sticks ---
            // std::abs() gives us the absolute value so we can compare magnitude
            // without caring about direction (positive or negative).
            if (std::abs(leftX) < DEAD_ZONE) leftX = 0;
            if (std::abs(leftY) < DEAD_ZONE) leftY = 0;

            // --- Add stick input to player velocity ---
            // Dividing by 32767.f normalizes to the -1.0..1.0 range.
            // Multiplying by PLAYER_SPEED scales to pixels per second.
            // Adding to playerVelocity means keyboard and stick input stack
            // both work at the same time.
            playerVelocity.x += (leftX / 32767.f) * PLAYER_SPEED;
            playerVelocity.y += (leftY / 32767.f) * PLAYER_SPEED;

            // --- A button plays sound ---
            // SDL_GameControllerGetButton returns 1 every frame while held,
            // so this will spam sound.play(). For a real game you'd track the
            // previous frame's button state and only trigger on the transition
            // from 0 → 1. Fine for sandbox verification.
            if (buttonA)
                hitSound.play();

            // --- Right trigger fires rumble ---
            // SDL_GameControllerRumble(controller, low_freq, high_freq, duration_ms)
            // low_freq  = left motor (bass/thud feel)   range 0..65535
            // high_freq = right motor (buzz/vibration)  range 0..65535
            // Only triggers when trigger is pressed past halfway (> 16383).
            if (rightTrigger > 16383)
                SDL_GameControllerRumble(controller, 32000, 32000, 100);

        }

        // =====================================================================
        // STEP 9G: UPDATE MOVE SPRITE
        // sprite.move() adds an offset to the sprite's current position.
        // Multiplying velocity by dtSeconds makes movement frame-rate independent:
        // at 60fps, dtSeconds ≈ 0.016, so velocity of 300 moves 300*0.016 = ~5px
        // per frame, which is 300 pixels per second regardless of frame rate.
        // =====================================================================
        playerSprite.move(playerVelocity * dtSeconds);

        // =====================================================================
        // STEP 9H: IMGUI UI
        // All ImGui widget calls go between ImGui::SFML::Update() and
        // ImGui::SFML::Render(). Think of this block as "defining" the UI
        // for this frame ImGui doesn't draw anything to the screen yet here.
        //
        // DockSpaceOverViewport MUST be called before any ImGui::Begin() calls.
        // It creates an invisible full-window host that other panels can dock into.
        // ImGuiDockNodeFlags_PassthruCentralNode makes the center area transparent
        // so our SFML sprite/world shows through the undocked center region.
        // =====================================================================
        ImGui::DockSpaceOverViewport(
            0,
            ImGui::GetMainViewport(),
            ImGuiDockNodeFlags_PassthruCentralNode
        );

        // --- Debug Panel ---
        // ImGui::Begin() opens a panel. ImGui::End() closes it.
        // Everything between them is drawn inside that panel.
        ImGui::Begin("Debug");

            // ImGui::Text() works like printf you can embed variable values.
            sf::Vector2f pos = playerSprite.getPosition();
            ImGui::Text("Sprite Position:  X: %.1f  Y: %.1f", pos.x, pos.y);

            sf::Vector2f camCenter = camera.getCenter();
            ImGui::Text("Camera Center:    X: %.1f  Y: %.1f", camCenter.x, camCenter.y);

            ImGui::Separator(); // draws a horizontal dividing line

            // SliderFloat modifies the variable via a pointer (&soundVolume).
            // Dragging the slider changes soundVolume and we apply it below.
            if (ImGui::SliderFloat("Sound Volume", &soundVolume, 0.f, 100.f))
                hitSound.setVolume(soundVolume); // only called when slider changes

            // ImGui::Button() returns true for one frame when clicked.
            if (ImGui::Button("Play Sound"))
                hitSound.play();

            ImGui::Separator();

            // ImGui::Checkbox() toggles a bool via pointer.
            ImGui::Checkbox("Show ImGui Demo Window", &showDemoWindow);

            ImGui::Separator();

            // Controller status readout inside the panel.
            if (controller != nullptr)
            {
                ImGui::Text("Controller: %s", SDL_GameControllerName(controller));

                Sint16 lx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
                Sint16 ly = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
                ImGui::Text("Left Stick:  X: %d  Y: %d", lx, ly);

                Sint16 rt = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
                ImGui::Text("Right Trigger: %d", rt);
            }
            else
            {
                ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "No controller connected.");
            }

        ImGui::End(); // always pair every Begin() with an End()

        // Optionally show the full ImGui demo window for widget exploration.
        // ImGui::ShowDemoWindow() is a built-in function that displays hundreds
        // of example widgets great for learning what's available.
        if (showDemoWindow)
            ImGui::ShowDemoWindow(&showDemoWindow);

        // =====================================================================
        // STEP 9I: RENDER
        // Clear → draw SFML content → draw ImGui on top → flip buffer.
        //
        // window.clear() fills the entire screen with a color, erasing last frame.
        // window.draw() submits a drawable (sprite, shape, etc.) to the back buffer.
        // ImGui::SFML::Render() submits all the ImGui geometry on top of SFML.
        // window.display() swaps the back buffer to the screen (double buffering).
        //
        // ORDER MATTERS:
        //   clear() must be first
        //   SFML draws come before ImGui::SFML::Render()
        //   display() must be last
        // =====================================================================
        window.clear(sf::Color(30, 30, 30)); // dark grey background
        window.draw(playerSprite);
        ImGui::SFML::Render(window);
        window.display();
    }

    // =========================================================================
    // SECTION 10: CLEANUP
    // Release resources in reverse order of creation.
    // ImGui::SFML::Shutdown() cleans up the font atlas and OpenGL resources.
    // SDL_GameControllerClose() releases the controller handle.
    // SDL_Quit() shuts down all SDL subsystems.
    // The SFML window and audio resources are destroyed automatically when
    // they go out of scope (C++ RAII destructors handle it).
    // =========================================================================
    ImGui::SFML::Shutdown();

    if (controller != nullptr)
        SDL_GameControllerClose(controller);

    SDL_Quit();

    spdlog::shutdown(); 

    return 0;
}

// =============================================================================
// CONTROLS REFERENCE
// =============================================================================
//   WASD          move sprite (keyboard)
//   Arrow Keys    pan camera
//   Z             zoom camera in
//   X             zoom camera out
//   Space         play sound effect
//   Escape        close window
//   Left Stick    move sprite (controller)
//   A Button      play sound effect (controller)
//   Right Trigger rumble
// =============================================================================
