// bootstrap.cpp  (compile with -shared)
#include "kernel.hpp"
#include <iostream>

extern "C" __declspec(dllexport) void zen_program_entry(zen::Kernel& k) {
    // Provide the tiniest sense possible
    static float fake_dt = 0.016f;
    k.provide("TimeDriver.delta_time", [] { return &fake_dt; });

    // Provide a shutdown sense (for fun/testing)
    k.provide("System.shutdown", [] {
        return ((void(*)())([]() { std::cout << "Shutdown requested!\n"; exit(0); }));
    });

    std::cout << "[bootstrap] Registered basic senses\n";
}
