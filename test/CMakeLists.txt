# sfml learning
add_executable(sfml_sandbox src/sfml_sandbox.cpp)
target_link_libraries(sfml_sandbox PRIVATE
  SFML::Graphics
  SFML::Audio
  ImGui-SFML::ImGui-SFML
  SDL2::SDL2
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