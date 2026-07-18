// main.cpp
#include "kernel.hpp"
#include <iostream>
#include <string>

int main() {
  auto& k = zen::Kernel::instance();

  std::cout << "Zen Kernel v0.0 booting...\n";

  // Bootstrap: load the very first program (you compile this one manually first)
  // Assume we have "bootstrap.dll" / "libbootstrap.so" built from bootstrap.cpp
  if (!k.load_program("./bootstrap.dll")) {  // adjust path/extension
    std::cerr << "Bootstrap failed — kernel cannot start\n";
    return 1;
  }
  if (!k.load_program("./console.dll")) {  // adjust path/extension
    std::cerr << "Console failed, using fallback" << std::endl;
    return 1;
  }

  // Simple REPL loop (will be replaced by terminal program later)
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "exit") break;
    if (line == "dump") {
      k.dump_senses();
    } else if (line.rfind("load ", 0) == 0) {
      std::string path = line.substr(5);
      k.load_program(path);
    } else {
      std::cout << "Unknown command. Try: dump, load <path>, exit\n";
    }
  }

  std::cout << "Kernel shutdown\n";
  return 0;
}