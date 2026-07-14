# sfml learning
add_executable(sfml_sandbox src/sfml_sandbox.cpp)
target_link_libraries(sfml_sandbox PRIVATE
  SFML::Graphics
  SFML::Audio
  ImGui-SFML::ImGui-SFML
  SDL2::SDL2
  spdlog::spdlog
)

target_link_options(sfml_sandbox PRIVATE -mconsole)
add_custom_command(TARGET sfml_sandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:sfml_sandbox>
    $<TARGET_FILE_DIR:sfml_sandbox>
  COMMAND_EXPAND_LISTS
)

add_custom_command(TARGET sfml_sandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:sfml_sandbox>/assets
)

add_custom_command(TARGET sfml_sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:sfml_sandbox>/logs
)

# json test
add_executable(json_test src/json_test.cpp)
target_link_libraries(json_test PRIVATE nlohmann_json::nlohmann_json)
target_link_options(json_test PRIVATE -mconsole)