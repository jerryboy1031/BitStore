#include "../include/Server.h"

int main() {
    // Port 6379 is standard for Redis, let's use 8080 for testing
    Server server(8080);
    server.Run();
    return 0;
}

/*
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
int main() {
    std::cout << "BitStore v0.1 - Type 'exit' to quit" << std::endl;
    std::stringstream ss ; //init sstream
    std::unordered_map<std::string,std::string> database;
    // The Main Loop
    while (true) {
        // 1. PROMPT----------------------------
        std::cout << "> ";

        // 2. READ------------------------------------
        std::string input_line;
        // std::getline reads until the user hits Enter (handling spaces)
        if (!std::getline(std::cin, input_line)) {
            break; // Handle Ctrl+D (EOF) to exit gracefully
        }
        // Check for exit command
        if (input_line == "exit") break;
        // If empty input, just loop again
        if (input_line.empty()) continue;


        // 3. PARSE (Tokenization)---------------------------------------
        ss.str(""); //change contents to empty
        ss.clear(); //delete EOF of last input
        ss << input_line ; //已經getline後，split a string by spaces in C++.
        std::string segment;
        std::vector<std::string> tokens;

        // 'ss >> segment' splits the string by whitespace automatically
        while (ss >> segment) tokens.push_back(segment);
  
        if (tokens.empty()) continue;
        
        // COMMAND HANDLING
        std::string command = tokens[0];

        // --- SET command ---
        if (command == "SET") {
            if (tokens.size() < 3) {
                std::cout << "Error: SET requires a key and a value" << std::endl;
            } else {
                std::string key = tokens[1];
                std::string value = tokens[2];
                
                // Store it!
                database[key] = value; 
                std::cout << "OK" << std::endl;
            }
        }
        // --- GET command ---
        else if (command == "GET") {
            if (tokens.size() < 2) {
                std::cout << "Error: GET requires a key" << std::endl;
            } else {
                std::string key = tokens[1];

                // Check if key exists
                if (database.find(key) != database.end()) {
                    std::cout << database[key] << std::endl;
                } else {
                    std::cout << "(nil)" << std::endl; // Standard redis-style response
                }
            }
        }
        else {
            std::cout << "Error: Unknown command '" << command << "'" << std::endl;
        }


        
    }

    return 0;
}
*/