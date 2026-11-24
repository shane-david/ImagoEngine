# Getting Started

Build a small program with a player you can move around and a key that reports where it is. 

---

This file includes how to configure a game, define a scene, and create an entity in the current code-only version of ImagoEngine. Before you start see the ImagoEngine build and links in your project (see the main README). 

NOTE: ImagoEngine cannot draw anything yet, rendering is the next step, for now the game window stays empty, so this example prints the player’s position to the console instead. 

---

## Step 1: Configure and Create the Game

Every ImagoEngine program starts in `main` by filling in a `GameConfig` and creating a `Game` from it:

```cpp
#include "Imago/Engine/Game.hpp"

using namespace Imago::Engine; 

int main()
{
	// create the config and define its members
	GameConfig config; 
	config.title = "ImagoEngine Game";
	config.width = 1680; 
	config.height = 900; 
	config.firstScene = "Playground"; 
	
	// create the game from the config
	Game game(config); 
}
```

By default the window is 1280x720 and runs in sync with your monitor. 

## Step 2: Define your first Scene

A scene is one self-contained part of your game. Create one by inheriting from `Scene`: 

```cpp
using Imago::ECS::Entity; 

class Playground : public Scene {
public:
	using Scene::Scene; // required 
	
protected:
	void on_load() override 
	{
		// setup logic 
	}
};
```

Then register it in `main`, using the same name you gave `firstScene`:

```cpp
game.register_scene<Playground>("Playground"); 
```

## Step 3: Add a Player

Components are plain structs that hold data. Define three: a position, a velocity, and an empty tag that marks which entity is the player. 

```cpp
struct cPosition { float x = 0.f, y = 0.f; }; 
struct cVelocity { float x = 0.f, y = 0.f; }; 
struct cPlayer {}; 
```

In `on_load`, create an entity and bind the components to it:

```cpp
void on_load() override 
{
	Entity player = _nexus.create(); 
	_nexus.bind<cPlayer>(player, {}); 
	_nexus.bind<cPosition>(player, {640.0f, 360.0f}); 
	_nexus.bind<cVelocity>(player, {}); 
}
```

## Step 4: Set up input

Create actions in `main` and add inputs to them. This example adds both keyboard keys and gamepad stick to each direction:

```cpp
game.register_action("left");
game.add_input("left", sf::Keyboard::Scancode::A);
game.add_input("left", GamepadAxis::LeftStickLeft);

game.register_action("right");
game.add_input("right", sf::Keyboard::Scancode::D);
game.add_input("right", GamepadAxis::LeftStickRight);

game.register_action("up");
game.add_input("up", sf::Keyboard::Scancode::W);
game.add_input("up", GamepadAxis::LeftStickUp);

game.register_action("down");
game.add_input("down", sf::Keyboard::Scancode::S);
game.add_input("down", GamepadAxis::LeftStickDown);

game.register_action("report");
game.add_input("report", sf::Keyboard::Scancode::Space);
game.add_input("report", GamepadButton::FaceDown);

game.register_action("quit");
game.add_input("quit", sf::Keyboard::Scancode::Escape);
game.add_input("quit", GamepadButton::Start);
```

## Step 5: Write the Systems

Systems are free functions that hold you game’s logic. For this example, write these four:

```cpp
// turns input into velocity
void PlayerInputSystem(SceneContext& ctx, float dt) 
{
	// get the direction from the actions
	glm::vec2 dir = ctx.input->direction("left", "right", "up", "down"); 
	
	// only the player entity 
	for (Entity e : ctx.nexus->survey<cPlayer, cVelocity>()) {
		cVelocity& vel = ctx.nexus->survey<cPlayer, cVelocity>(e); 
		vel.x = dir.x * 200.f;
		vel.y = dir.y * 200.f; 
	}
}

// applies velocity to position
void MoveSystem(SceneContext& ctx, float dt) 
{
	// only the player entity
	for (Entity e : ctx.nexus->survey<cPosition, cVelocity>()) {
		cPosition& pos = ctx.nexus->get<cPosition>(e); 
		const cVelocity& vel = ctx.nexus->get<cVelocity>(e); 
		pos.x += vel.x * dt;
		pos.y += vel.y * dt; 
	}
}

// prints the player's postion when "report" is pressed
void ReportSystem(SceneContext& ctx, float dt) 
{
	// on began because we only want to print on the frame it is pressed
  if (!ctx.input->began("report")) return;
  
  // only the player entity
  for (Entity e : ctx.nexus->survey<cPlayer, cPosition>()) {
      const cPosition& pos = ctx.nexus->get<cPosition>(e);
      spdlog::info("Player is at ({:.0f}, {:.0f})", pos.x, pos.y);
  }
}

// ends the game when "quit" is pressed
void QuitSystem(SceneContext& ctx, float dt)
{
	if (ctx.input->began("quit")) {
		ctx.engine->quit(); 
	}
}
```

Register them in `on_load`: 

```cpp
_scheduler.register_update(PlayerInputSystem);
_scheduler.register_fixed(MoveSystem);
_scheduler.register_update(ReportSystem);
_scheduler.register_update(QuitSystem);
```

`MoveSystem` is a **fixed** system, because movement is something game rules depend on, so it should run at the same rate on every machine. the other three respond to the player, so they are **frame** systems that run once per frame. 

## Step 6: Run it

```cpp
game.run(); 
```

When you run the program, an empty window opens. Move with WASD or the left stick, then press space or the gamepad’s bottom face button to print the player’s position to the console. It changes as you move. Press escape or start or close the window to quit.