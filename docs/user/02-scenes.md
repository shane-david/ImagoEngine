# Scenes

Split your game into self-contained parts: a menu, a level, or a game over screen, ect. 

---

Use this when you are structuring a new part of your game that has its own entities, systems, and rules. 

Include: `Imago/Engine/Scene.hpp`

---

## Example

```cpp
class Level : public Scene {
public: 
	using Scene::Scene; 
protected:
	
	void on_load() override 
	{
		Entity player = _nexus.create(); 
		_nexus.bind<cPlayer>(player, {}); 
		_nexus.bind<cPosition>(player, {100.f, 300.f}); 
		_nexus.bind<cVelocity>(player {}); 
		
		_scheduler.register_update(PlayerMoveSystem); 
		_scheduler.register_fixed(GravitySystem); 
		_scheduler.register_fixed(MoveSystem); 
		
		_broadcaster.subscribe<PlayerDied>(OnPlayerDied); 
	}
}; 
```

## How it Works

A **scene** is one self-contained part of your game. Each scene has its own entities, systems, and events. Only one scene runs at a time. When the player moves from the main menu into a level , the game switches from one scene to another. 

You create one scene by writing a class that inherits from `Scene`. In it, you fill in the `on_load` method which runs when the scene starts. `on_load` ought to: define the scene’s starting entities, register its systems, and subscribe to its events. After that, the engine runs the current active scene’s every frame until you switch to another scene. 

A scene class describes the **kind** of scene and its rules. Keep game logic in systems, not in the scene class itself. 

## Scene Template

```cpp
#include "Imago/Engine/Scene.hpp"

using namespace Imago::Engine;
using Imago::ECS::Entity; 

class MyScene : public Scene {
public:

	// required for scene construction - do not remove 
	using Scene::Scene; 
	
protected:
	
	void on_load() override 
	{
		// create entities, register systems, subscribe to events
	}
	
	void on_unload() override 
	{
		// option clean up logic: the engine automatically takes care of most 
	}
}; 
```

You should only ever write code inside of `on_load` or `on_unload`. If you do not need `on_unload` it can be safely deleted but `on_load` must be defined. 

## Common Tasks

### Registering scenes and choosing the first one

Register every scene with the game at once, before calling `run()`, and give each one a name. Set `firstScene` to the scene the game should start in. 

```cpp
int main() 
{
    GameConfig config;
    config.title = "My Game";
    config.firstScene = "MainMenu";

    Game game(config);
    game.register_scene<MainMenu>("MainMenu");
    game.register_scene<Level>("Level");
    game.register_scene<GameOver>("GameOver");

    game.run();
}
```

From then on, every scene is referred to by its name given here, not its type. 

### Setting up a scene in `on_load`

`on_load` is the only place you set a scene up. Use the scene’s members:

| Member | Use it to | See |
| --- | --- | --- |
| `_nexus` | Create entities and bind components | [Entities and Components](https://app.notion.com/p/03-entities-and-components.md) |
| `_scheduler` | Register systems | [Systems](https://app.notion.com/p/05-systems.md) |
| `_broadcaster` | Subscribe to events | [Events](https://app.notion.com/p/06-events.md) |

Everything you `bind` is in place before the scene’s first frame.

### Cleaning up in `on_unload`

`on_unload` runs just before the scene ends. Most scenes will not need it because the scene’s entities, systems, and subscriptions are all removed automatically. Use it only for work outside the scene. 

```cpp
void on_unload() override
{
	// log that a scene is being switched 
}
```

### Switching to another scene

Scenes are switched by name from inside a system. See {**game notifications}**

## API Summary

| Name | What it is | Notes |
| --- | --- | --- |
| `game.register_scene<T>(name)` | Registers scene class `T` under `name` | Call before `run()`; each name once |
| `config.firstScene` | The name of the scene the game starts in | Must be a registered name |
| `on_load()` | Your scene's setup | Required |
| `on_unload()` | Runs when your scene ends | Optional |
| `_nexus`, `_scheduler`, `_broadcaster` | The scene's entities, systems, and events | Available in `on_load` and `on_unload` |

## General Guidelines

- Do not delete `using Scene::Scene`. This line allows the scene’s you defined to be constructed properly, deleting it will cause a compiler error.
- Do not add you own constructor to do set up, and do not give you scene constructor parameters. Do all set up in `on_load` and store all per-scene data in components.
- A scene is only created when it is first switched to, not when you call `register_scene<T>`. Expect a short pause when you enter a large scene for the first time.
- `on_load` runs every time a scene starts fresh, including switching back to a scene that did not persist.
- Scenes do not share entitles, each set has its own entities and events.
- Do not call `load()`, `update()`, or `unload()` yourself, simply create and register your scene and the engine will handle the rest.

## See Also

[Getting Started](01-getting-started.md)

[Engine Requests](08-engine-requests.md)

[Systems](05-systems.md)