# sfml learning
add_executable(sfml_sandbox src/sfml_sandbox.cpp)
target_link_libraries(sfml_sandbox PRIVATE
  SFML::Graphics
)

target_link_options(sfml_sandbox PRIVATE -mconsole)
add_custom_command(TARGET sfml_sandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_RUNTIME_DLLS:sfml_sandbox>
    $<TARGET_FILE_DIR:sfml_sandbox>
  COMMAND_EXPAND_LISTS
)