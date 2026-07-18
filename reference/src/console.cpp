// console.cpp  (compile as SHARED library)
#include "kernel.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

extern "C" __declspec(dllexport) void zen_program_entry(zen::Kernel& k) {
    // === Callable senses (functions with parameters) ===

    // Console.print(string)
    static auto print_fn = [](const std::string& msg) {
        std::cout << "[ZEN] " << msg << std::endl;
    };
    k.provide("Console.print", []() {
        return (void(*)(const std::string&))print_fn;
    });

    // Console.read_line() → string
    static auto read_line_fn = []() -> std::string {
        std::string line;
        std::getline(std::cin, line);
        return line;
    };
    k.provide("Console.read_line", []() {
        return (std::string(*)())read_line_fn;   // raw function pointer in any
    });

    // System.senses (for dump)
    k.provide("System.list_senses", []() {
        return (void(*)())([]() {
            zen::Kernel::instance().dump_senses();
        });
    });

    std::cout << "[console] BIOS prompt loaded — type 'help' for commands\n";

    // === Start the interactive BIOS loop (takes over from main) ===
    std::string line;
    while (true) {
        std::cout << "zen> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "exit" || cmd == "quit") {
            break;
        }
        else if (cmd == "help") {
            std::cout << "Commands: help | senses | load <path> | unload <name> | call <sense> [args] | exit\n";
        }
        else if (cmd == "senses" || cmd == "dump") {
            zen::Kernel::instance().dump_senses();
        }
        else if (cmd == "load") {
            std::string path;
            iss >> path;
            if (!path.empty()) {
                zen::Kernel::instance().load_program(path);
            }
        }
        else if (cmd == "unload") {
            std::string name;
            iss >> name;
            if (!name.empty()) {
                zen::Kernel::instance().unload_program(name);
            }
        }
        else if (cmd == "call") {
            std::string sense_name;
            iss >> sense_name;
            // For v0: simple parameter handling (string only for now)
            if (sense_name == "Console.print") {
                std::string msg;
                std::getline(iss >> std::ws, msg);  // rest of line
                auto print = zen::Kernel::instance().get_sense<void(*)(const std::string&)>("Console.print");
                if (print) (*print)(msg);
            }
            else if (sense_name == "System.list_senses") {
                auto dump = zen::Kernel::instance().get_sense<void(*)()>("System.list_senses");
                if (dump) (*dump)();
            }
            // Add more call patterns as we grow
        }
        else {
            std::cout << "Unknown command. Type 'help'\n";
        }
    }

    std::cout << "[console] BIOS prompt exiting\n";
}
