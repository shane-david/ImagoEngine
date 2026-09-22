add_library(ImagoEngineCore
    src/ECS/EntityManager.cpp
    src/ECS/Nexus.cpp
)

target_include_directories(ImagoEngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ImagoEngineCore PUBLIC
    glm::glm
    spdlog::spdlog
)