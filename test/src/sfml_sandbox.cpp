// SFML Learning has the following development goals
// set up basic SFML window in the sandbox project using CMake set up
// laod a texture from disk and render a sprite to the screen
// implemenet basic keyboard input using SFML to move the sprite around 

#include <SFML/Graphics.hpp>
#include <iostream> 

int main() {

    // create the window 800x600 pixels
    sf::RenderWindow window(sf::VideoMode({800,600}), "SFML Sandbox"); 

    // set up the texture
    sf::Texture player; 

    // try to load the texture, if it fails, throw an error 
    if (!player.loadFromFile("C:/Dev/ImagoEngine/test/assets/player.png")) {
        std::cerr << "ERROR: Could not load texture at: " << "C:/Dev/ImagoEngine/test/assets/player.png" << std::endl; 
        return -1; 
    }

    // create a sprite from the texture and set its origin to the center of the texture
    sf::Sprite spr_player(player); 

    // set the origin to the center
    sf::FloatRect bounds = spr_player.getLocalBounds(); 
    spr_player.setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f}); 

    // declare variable to track for movement
    sf::Vector2f playerVel = {0, 0}; 
    float playerSpeed = 300; // per second

    // keep track of clock for delta time 
    sf::Clock clock; 

    // GAME LOOP 
    while (window.isOpen()) {

        // EVENT HANDLING
        while (const std::optional event = window.pollEvent()) {

            // if the event is closed 
            if (event->is<sf::Event::Closed>()) {

                window.close(); 

            // if a key is pressed 
            } else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {

                // if the key is the escape key close the window 
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close(); 
                }
            }
        }

        // set the velocity back to 0 at the beginning of each loop
        playerVel = {0.f, 0.f}; 

        // INPUT HANDLING
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::W)) {
            playerVel.y -= playerSpeed; 
        } 
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::A)) {
            playerVel.x -= playerSpeed; 
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::S)) {
            playerVel.y += playerSpeed; 
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::D)) {
            playerVel.x += playerSpeed; 
        }

        // UPDATE 

        // get delta time from the clock
        sf::Time dt = clock.restart(); 

        // move sprite by velocity times delta time 
        spr_player.setPosition(spr_player.getPosition() + (playerVel*dt.asSeconds())); 

        //RENDER
        window.clear(sf::Color::Black);
        window.draw(spr_player); 
        window.display(); 

    }

}