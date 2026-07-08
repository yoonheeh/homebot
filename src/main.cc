#include <iostream>

int main() {
#if defined(__aarch64__)
  std::cout << "Running on ARM64 (aarch64) architecture." << std::endl;
#elif defined(__x86_64__)
  std::cout << "Running on x86_64 architecture." << std::endl;
#else
  std::cout << "Running on an unknown architecture." << std::endl;
#endif
  return 0;
}
