# Library List

Author: Shane David
Type: Design
Upload Date: March 11, 2026

- `SFML` - rendering and core, this library will handle the window creations, rendering, keyboard input, mouse input, audio and assets.
- `SDL2` - this library will only include the Game Controller API to make up for where SFML is lacking to allow full controller support including rumble, mapping, and haptic feedback.
- `Dear ImGui` - this is the main user interface library that will be used to create the editor and the debug UI
- `nlohmann/json` - this will be used for the JSON serialization pipeline in order for saving levels and putting them into scenes
- `yaml-cpp` - this will be used to create YAML config files and engine setting. This is for things that the user will interact with directly as YAML files are more human readable.
- `spdlog` - will be used for error and debug logging
- `GLM` - will be used for vectors, matrices, transforms, and more complicated mathematical algorithms
- `Catch2` - will be used for unit testing physics, ECS, and engine systems
- `stb_image` - will be used for additional image support to make up for where SFML is lacking
- `fmt` - will be used for string formatting for the UI
