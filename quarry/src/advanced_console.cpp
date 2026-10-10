// advanced_console.cpp  (SHARED lib)
#include "kernel.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <conio.h>     // _getch() for Windows key reading
#include <algorithm>

extern "C" __declspec(dllexport) void zen_program_entry(zen::Kernel& k) {
    std::cout << "[advanced] BIOS prompt v2 loaded — TAB for auto-complete\n";

    // Overwrite/re-provide core senses with improved versions if desired
    k.provide("Console.print", []() -> zen::Sense {
        static auto fn = [](const std::string& msg) {
            std::cout << "[ADV] " << msg << std::endl;
        };
        return (void(*)(const std::string&))fn;
    });

    // List of commands for auto-complete
    std::vector<std::string> commands = {
        "help", "senses", "dump", "load", "unload", "call", "replace", "exit", "quit"
    };

    auto get_completions = [&](const std::string& prefix) -> std::vector<std::string> {
        std::vector<std::string> matches;
        for (const auto& cmd : commands) {
            if (cmd.rfind(prefix, 0) == 0) {
                matches.push_back(cmd);
            }
        }
        return matches;
    };

    std::string line;
    size_t cursor_pos = 0;
    size_t completion_index = 0;
    std::vector<std::string> current_completions;

    while (true) {
        std::cout << "zen> " << line;
        if (cursor_pos < line.size()) {
            // Simulate cursor (crude, redraw line)
            std::cout << std::string(line.size() - cursor_pos, '\b');
        }
        std::cout.flush();

        int ch = _getch();  // non-echo key

        if (ch == 9) {  // TAB
            if (current_completions.empty()) {
                // First TAB: generate completions based on current word
                size_t space = line.find_last_of(" \t");
                std::string word = (space == std::string::npos) ? line : line.substr(space + 1);
                current_completions = get_completions(word);
                completion_index = 0;
            }

            if (!current_completions.empty()) {
                // Cycle / apply completion
                std::string completion = current_completions[completion_index];
                // Replace last word with completion
                size_t space = line.find_last_of(" \t");
                if (space != std::string::npos) {
                    line.replace(space + 1, std::string::npos, completion);
                } else {
                    line = completion;
                }
                cursor_pos = line.size();
                completion_index = (completion_index + 1) % current_completions.size();

                // Redraw
                std::cout << "\rzen> " << line << std::flush;
            }
            continue;
        }
        else if (ch == 13) {  // Enter
            std::cout << std::endl;
            // Reset completions
            current_completions.clear();
            completion_index = 0;

            // Process command (same as before, but add 'replace')
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "replace") {
                std::string new_path;
                iss >> new_path;
                if (!new_path.empty()) {
                    std::cout << "[advanced] Attempting replace with: " << new_path << "\n";
                    bool success = k.load_program(new_path);
                    if (success) {
                        std::cout << "[advanced] Replacement loaded — exiting old console\n";
                        break;  // Exit loop → old console gone, new one takes over
                    } else {
                        std::cout << "[advanced] Load failed — staying alive\n";
                    }
                }
            }
            else if (cmd == "help") {
                std::cout << "Commands: help | senses | load <path> | unload <name> | call <sense> [args] | replace <dll_path> | exit\n";
                std::cout << "TAB auto-complete for commands\n";
            }
            else if (cmd == "exit" || cmd == "quit") {
                break;
            }
            else {
                // Reuse old logic or forward to other senses
                std::cout << "Processing: " << line << "\n";
                // ... (add call, load, etc. as before)
            }

            line.clear();
            cursor_pos = 0;
        }
        else if (ch == 8 && !line.empty() && cursor_pos > 0) {  // Backspace
            line.erase(--cursor_pos, 1);
            std::cout << "\b \b";  // Erase char
        }
        else if (ch >= 32 && ch <= 126) {  // Printable char
            line.insert(cursor_pos++, 1, static_cast<char>(ch));
            std::cout << static_cast<char>(ch);
        }
        // Ignore other keys for now
    }

    std::cout << "[advanced] Exiting — replacement complete or shutdown\n";
}
