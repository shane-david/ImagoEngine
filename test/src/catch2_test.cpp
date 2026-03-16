#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

int Add(int a, int b) {
  return a + b;
}

TEST_CASE("Addition works correctly", "[math]") {
  REQUIRE(Add(2, 3) == 5);
  REQUIRE(Add(-1, 1) == 0);
  REQUIRE(Add(0, 0) == 0);
}

int main(int argc, char* argv[]) {
  return Catch::Session().run(argc, argv);
}