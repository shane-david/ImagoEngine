add_library(ImagoEngineCore
    src/ECS/EntityManager.cpp
    src/ECS/Nexus.cpp
    src/ECS/CommandBuffer.cpp
    src/ECS/Bond.cpp
    src/Engine/Broadcaster.cpp
    src/Engine/SystemScheduler.cpp
    src/Engine/Scene.cpp
    src/Engine/SceneManager.cpp
    src/Engine/EngineControl.cpp
    src/Engine/Game.cpp
    src/Engine/InputState.cpp
    src/Engine/InputHandler.cpp
)

target_compile_options(ImagoEngineCore PRIVATE -Wall -Wextra)

target_include_directories(ImagoEngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ImagoEngineCore PUBLIC
    glm::glm
    spdlog::spdlog
    SFML::Graphics
    SFML::Window
    SFML::System
)

target_link_libraries(ImagoEngineCore PRIVATE SDL2::SDL2)