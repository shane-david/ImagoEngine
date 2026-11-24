# Engine Requests

Direct all engine-wide requests: switching scenes and quitting the game. 

---

Use this when you need to switch scenes or quit the game: the player starts a level, opens a pause menu, dies, wins, quits, ect. 

Include: `Imago/Engine/Scene.hpp`

---

## Example

```cpp
void OnPlayerDied(SceneContext& ctx, const PlayerDied&) 
{
	ctx.engine->switch_scene("GameOver"); 
}
```

## How it Works

Systems can control the game itself through `ctx.engine`. It currently has two functions:

- `ctx.engine->switch_scene("ScaneName");` - go to another scene
- `ctx.engine->quit()` - quit the game

Neither one happens immediately. Both are **requests** that the engine carries out at the end of the current frame, after every system has finished. The rest of the frame runs normally, so it is always safe to call them from anywhere in a system, even in the middle of a loop. 

Scenes are switched by name, specifically the name you gave to each scene when you registered it (**See [Scenes](02-scenes.md)**). 

## Common Tasks

### Leaving a scene without persisting

By default, the scene you are leaving is removed, its `on_unload` runs, and all its entities, systems, and subscriptions are destroyed. The next time you switch to it, it starts fresh with `on_load`. 

```cpp
// the current scene is removed
ctx.engine->switch_scene("MainMenu"); 
```

### Keeping a scene waiting

Pass `true` as the second argument to keep the scene you are leaving in memory. It stops running but keeps everything exactly as it was. When you switch back to it, it picks up where it left off, and `on_load` does not run again. 

```cpp
// keeps the current scene waiting
ctx.engine->switch_scene("Pause", true); 
```

`true` always refers to the scene that you are leaving, not the one that you are going to:

```cpp
// in the level
if (ctx.input->began("inventory")) {
	ctx.engine->switch_scene("Inventory", true); // keeps the current level waiting
}

// in the inventory
if (ctx.input->began("close")) {
	ctx.engine->switch_scene("Level"); // back to level in current state, inventory is destroyed
}
```

### Restarting the current scene

If you switch to the scene that you are currently in it is removed and started fresh with `on_load` even if you pass `true`:

```cpp
void OnPlayerDied(SceneContext& ctx, const PlayerDied&) 
{
	// restarts it if called from inside "Level"
	ctx.engine->switch_scene("Level"); 
}
```

### Quitting

`quit()` ends the game at the end of the frame. Closing the game window does the same thing. Every scene, including any you kept waiting in memory, gets its `on_unload` called before the game closes:

```cpp
ctx.engine->quit(); 
```

## API Summary

| Function | What it does | Takes effect |
| --- | --- | --- |
| `ctx.engine->switch_scene(name)` | Goes to `name` and removes the current scene | End of frame |
| `ctx.engine->switch_scene(name, true)` | Goes to `name` and keeps the current scene waiting | End of frame |
| `ctx.engine->quit()` | Ends the game | End of frame |

## General Guidlines

- After a switch or quit request, every remaining system still runs in the frame and the actual switch or quit happens at the end of the frame.
- If several scene switches are requested in one frame then the last one happens.
- If a quit and a switch are both requested in the same frame the game quits and the switch never happens.
- Passing in `true` to `switch_scene` keeps the scene you are leaving in memory, not the one that you are going to.
- When you keep a scene waiting, it is completely paused, its systems do not run and nothing tells it that it was paused or resumed.
- Switching to the current scene restarts it whether you pass in `true` or not.
- Input presses do not carry over between scenes but button that are still held down stay held.
- The scene must be registered to switch to it.

## See Also

[Scenes](02-scenes.md)

[Input](07-input.md)

[Events](06-events.md)