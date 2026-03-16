# SFML test
add_executable(sfml_test src/sfml_test.cpp)
target_link_libraries(sfml_test PRIVATE
  SFML::Graphics
  SFML::Window
  SFML::System
)
target_link_options(sfml_test PRIVATE -mwindows)
add_custom_command(TARGET sfml_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:sfml_test>
    $<TARGET_FILE_DIR:sfml_test>
  COMMAND_EXPAND_LISTS
)

# SDL2 test
add_executable(sdl2_test src/sdl2_test.cpp)
target_link_libraries(sdl2_test PRIVATE
  SDL2::SDL2main
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

# ImGui test
add_executable(imgui_test src/imgui_test.cpp)
target_link_libraries(imgui_test PRIVATE
  ImGui-SFML::ImGui-SFML
  SFML::Graphics
  SFML::Window
  SFML::System
)

# nlohmann json test
add_executable(json_test src/json_test.cpp)
target_link_libraries(json_test PRIVATE
  nlohmann_json::nlohmann_json
)

# yaml-cpp test
add_executable(yaml_test src/yaml_test.cpp)
target_link_libraries(yaml_test PRIVATE
  yaml-cpp::yaml-cpp
)

# spdlog test
add_executable(spdlog_test src/spdlog_test.cpp)
target_link_libraries(spdlog_test PRIVATE
  spdlog::spdlog
)

# glm test
add_executable(glm_test src/glm_test.cpp)
target_link_libraries(glm_test PRIVATE
  glm::glm
)

# Catch2 test
add_executable(catch2_test src/catch2_test.cpp)
target_link_libraries(catch2_test PRIVATE
  Catch2::Catch2
)
target_link_options(catch2_test PRIVATE -mconsole)

# stb test
add_executable(stb_test src/stb_test.cpp)
target_include_directories(stb_test PRIVATE 
  ${Stb_INCLUDE_DIR}
  build/vcpkg_installed/x64-mingw-dynamic/include
)

# fmt test
add_executable(fmt_test src/fmt_test.cpp)
target_link_libraries(fmt_test PRIVATE
  fmt::fmt
)