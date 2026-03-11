#include <iostream>
#include <string>
#include <vector>

int Add(int a, int b) {
  return a + b;
}

// regular comment

//! alert comment

//? questoin comment

// TODO to comment

//* highlighted comment

// * this is struck through

// FIXME fix something that is broken

// BUG a known bug

// XXX needs more attention

// NOTE this is a note

int main() {
  int x      = 5;
  int y      = 10;
  int result = Add(x, y);

  std::vector<int> numbers = {1, 2, 3, 4, 5};

  for (int i = 0; i < numbers.size(); i++) {
    numbers[i] = numbers[i] * 2;
  }

  std::cout << "Result: " << result << std::endl;
  return 0;
}