# Input

Read the keyboard, mouse, and gamepad through named actions.

---

Use this when the player needs to control anything: moving, jumping, pausing, navigating menus, ect.

Include: `Imago/Engine/Game.hpp` and `Imago/Engine/Scene.hpp`

---

## Example

```cpp
// in main(), before game.run()
game.register_action("jump"); 
game.add_input("jump", sf::Keyboard::Scancode::Space);
game.add_input("jump", GamepadButton::FaceDown); 

// in a system
void JumpSystem(SceneContext& ctx, float dt)
{
	// only jump if the input just happened
	if (!ctx.input->began("jump")) return; 
	
	// jump logic
	for (Entity e : ctx.nexus->survey<cPlayer, cVelocity>()) {
		ctx.nexus->get<cVelocity>(e).y = -400.0f; 
	}
}
```

## How it Works

In ImagoEngine you do not check for specific keys, instead you created named **actions**, like `"jump"` or `"left"`, and add inputs to each one. Inputs currently could be a key, a mouse button, a gamepad button, or a direction on a stick. Systems then ask about the **action** instead of inputs:

```cpp
if (ctx.input->began("jump")) { // jump logic }
```

With this system, that line works for **any** input registered to the `"jump"` action, it could be Space, a mouse click, or a gamepad button. Your systems never need to know which device the player is using. 

Actions are set up once, in `main`, before the game runs. They are available in every scene. 

## Common Tasks

### Creating actions and adding inputs

Register each action, then add as many inputs to it as you like. Do this in `main`, after creating the `Game` and before calling `run()`. 

```cpp
game.register_action("fire"); 
game.add_input("fire", sf::Keyboard::Scancode::J); 
game.add_input("fire", sf::Mouse::Button::Left);
game.add_input("fire", GamepadButton::FaceRight);
game.add_input("fire", GamepadAxis::RightTrigger); 
```

Register an action before adding inputs to it. The kinds of inputs you can add include: 

| Input | Example | Notes |
| --- | --- | --- |
| Keyboard key | `sf::Keyboard::Scancode::Space` | By physical key position, so it works on any keyboard layout |
| Mouse button | `sf::Mouse::Button::Left` |  |
| Gamepad button | `GamepadButton::FaceDown` | Named by position: `FaceDown` is A on Xbox, X on PlayStation |
| Stick direction or trigger | `GamepadAxis::LeftStickLeft` | Each stick direction is separate |

### Checking an action

There are four questions you can ask about any action:

```cpp
// true the entire time the action is pressed
ctx.input->held("fire"); 

// true only on the frame or fixed step the action is first pressed
ctx.input->began("fire"); 

// true only on the frame or fixed step the action is let go
ctx.input->ended("fire"); 

// analog value from 0 to 1 representing how far it is pressed
ctx.input->strength("fire"); 
```

Keys and buttons always have a strength of 0 or 1. Triggers and sticks can be anything in between.

### Moving in two directions

`direction` combines four actions into one movement vector:

```cpp
glm::vec2 dir = ctx.input->direction("left", "right", "up", "down"); 
vel.x = dir.x * speed;
vel.y = dir.y * speed; 
```

The vector’s length is never more than 1, so moving diagonally is not faster than moving straight. A stick tilted partway gives a shorter vector, for slower movement. Positive `y` points down the screen. 

For a single axis, use `axis` instead:

```cpp
float x = ctx.input->axis("left", "right"); // -1 (left) to 1 (right)
```

### Using sticks and triggers

Each direction of a stick is its own input, so you add them to actions just like keys:

```cpp
game.add_input("left", GamepadAxis::LeftStickLeft);
game.add_input("right", GamepadAxis::LeftStickRight);
game.add_input("up", GamepadAxis::LeftStickUp);
game.add_input("down", GamepadAxis::LeftStickDown); 
```

Stick and trigger inputs take two optional: 

- **dead zone** (defaulted to `0.2`) - small movements below this are ignored, so a stick resting slightly off center does not move anything.
- **press threshold** (defaulted to `0.5`) - how far the stick or trigger must go before the action counts a **pressed** for `held`, `began`, and `ended`. `strength` will still report the exact amount below this point.

```cpp
// requires a light trigger press for aiming (lower press threshold)
game.add_input("aim", GamepadAxis::LeftTrigger, 0.05f, 0.1f); 

// requires a firm trigger press for firing (higher press threshold)
game.add_input("fire", GamepadAxiss::LeftTrigger, 0.05, 0.8f); 
```

### Looking up an action once

Every query by name looks the name up. In a system that checks the same action many times, look it up once and reuse the result:

```cpp
void FireSystem(SceneContext& ctx, float dt)
{
	// save the action's ActionId at the beginning of the system
	ActionId fire = ctx.input->action_id("fire)"; 
	
	for (Entity e : ctx.nexus->survey<cTurret>()) {
		if (ctx.input->held(fire)) {
			// firing logic
		}
	}
}
```

`action_id` returns `INVALID_ACTION` if the name does not exist. 

## API Summary

**Setup** in `main` before `game.run()`:

| **Function** | **What it does** | **Notes** |
| --- | --- | --- |
| `register_action(name)` | Creates an action | Returns its `ActionId` |
| `add_input(name, key)` | Adds a keyboard key | `sf::Keyboard::Scancode` |
| `add_input(name, button)` | Adds a mouse or gamepad button | `sf::Mouse::Button` or `GamepadButton` |
| `add_input(name, axis, deadZone, threshold)` | Adds a stick direction or trigger | Defaults: `0.2f, 0.5f` |

**In systems** on `ctx.input`:

| **Function** | **Returns** | **Notes** |
| --- | --- | --- |
| `held(action)` | `bool` | Currently pressed |
| `began(action)` | `bool` | Just pressed |
| `ended(action)` | `bool` | Just let go |
| `strength(action)` | `float` | 0 to 1 |
| `axis(negative, positive)` | `float` | -1 to 1 |
| `direction(left, right, up, down)` | `glm::vec2` | Length never above 1 |
| `action_id(name)` | `ActionId` | `INVALID_ACTION` if not found |

Every query accepts either a name or an `ActionId`. 

## General Guidelines

- Currently, actions are set up in `main` before `run()`. They are shared by every scene, scene’s can not add their own.
- Checking or adding an input to a name that was never registered is an error in debug builds.
- An action can have many inputs, if any of them are pressed the action is held and its strengths is the strongest of them.
- `held` and `strength` can disagree, a stick pushed only slightly has a strength above 0 but may not be past is press threshold and therefore not count as `held`.
- A quick tap still counts, if an input is pressed and released before you system runs, `began` and `ended` are both true while `held` is false.
- Presses do not carry into a new scene but buttons that are still held stay held.
- Everything is released when the window loses focus.
- After the window regains focus, a stick that is already pushed reads 0 until it moves.
- Currently there is only support for one player and one gamepad, the first connected gamepad is used and if it is unplugged the next connected controller takes over.
- Mouse position, scroll wheel, and changing bindings while the game runs are currently unsupported.

## See Also

[Systems](05-systems.md)

[Engine Requests](08-engine-requests.md)

[Getting Started](01-getting-started.md)