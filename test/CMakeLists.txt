# --- unit tests: every Catch2 test file in one executable ---
add_executable(imago_tests
    ECS/EntityTests.cpp
    ECS/EntityManagerTests.cpp
    ECS/SparseSetTests.cpp
    ECS/SurveyHandleTests.cpp
    ECS/NexusTests.cpp
    ECS/CommandBufferTests.cpp
    ECS/BondTests.cpp
    Engine/BroadcasterTests.cpp
    Engine/EngineControlTests.cpp
    Engine/GameTests.cpp
    Engine/InputStateTests.cpp
    Engine/InputHandlerTests.cpp
    Engine/SceneTests.cpp
    Engine/SceneManagerTests.cpp
    Engine/SystemSchedulerTests.cpp
)

# test/ itself is an include root so files can use "Support/EngineTestAccess.hpp"
target_include_directories(imago_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(imago_tests PRIVATE
    ImagoEngineCore
    Catch2::Catch2WithMain
)

target_link_options(imago_tests PRIVATE -mconsole)

# --- interactive demo of the user-facing API (its own main, no Catch2) ---
add_executable(imago_demo
    Demo/InteractiveDemo.cpp
)

target_link_libraries(imago_demo PRIVATE
    ImagoEngineCore
)

target_link_options(imago_demo PRIVATE -mconsole)
