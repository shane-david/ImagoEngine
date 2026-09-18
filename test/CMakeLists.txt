# ecs tests
add_executable(ecs_tests
  ECS/EntityManagerTests.cpp
  ECS/SparseSetTests.cpp
)

target_link_libraries(ecs_tests PRIVATE
  ImagoEngineCore
  Catch2::Catch2WithMain
)

target_link_options(ecs_tests PRIVATE -mconsole)