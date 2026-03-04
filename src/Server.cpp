#include "../include/Server.h"
#include <iostream>
#include <sys/socket.h> // Core socket functions
#include <netinet/in.h> // For sockaddr_in
#include <unistd.h>     // For close()
#include <cstring>      // For memset
#include <vector>
#include <sstream>
#include <fstream> // For file I/O
#include <thread> // Thread library
#include <chrono>
#include <cstdio>// For file renaming/deleting
#include <signal.h>
#include <algorithm> // <--- We will also need this for signal()

long long GetCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}


// --- Constructor & Destructor ---
Server::Server(int port) : port(port), server_fd(-1) {
    // NEW: Tell the OS not to crash our server if a client disconnects unexpectedly
    signal(SIGPIPE, SIG_IGN); //default behavior of SIGPIPE is to instantly kill your entire server program
    
    Load(); // 1. Replay the log first to rebuild memory
    
    // 2. Open the file in Append mode and keep it open!
    aof_file.open("bitstore.aof", std::ios::app);
    if (!aof_file.is_open()) {
        std::cerr << "[AOF] Error: Could not open AOF file for writing!" << std::endl;
    }
}

Server::~Server() {
    // Clean up when the server shuts down
    if (aof_file.is_open()) {
        aof_file.close();
    }
}

void Server::Run() {
    // 1. Create Socket
    // AF_INET = IPv4, SOCK_STREAM = TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket failed" << std::endl;
        return;
    }

    // 2. Bind to Port

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    /*
    and I want port 8080 (formatted for the network)." 
    Then, you hand that struct to the OS kernel via bind.
    */
    sockaddr_in address; //Socket Address for Internet
    address.sin_family = AF_INET; //IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Listen on 0.0.0.0 (Accept calls from any of my network cards)
    address.sin_port = htons(port);       // Host to Network Short (convert to BIG_Endianness)

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return;
    }

    // 3. Listen 看門狗: tell OS we are ready to accept connections
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return;
    }

    std::cout << "BitStore listening on port " << port << "..." << std::endl;

    // --- NEW: Start the Garbage Collector Thread ---
    std::thread gc_thread(&Server::GarbageCollector, this);
    gc_thread.detach(); // Let it run independently in the background, 

    // 4. Accept Loop
    while (true) {
        int addrlen = sizeof(address);
        // When the code hits accept(), it stops everything and sleeps, until a client connects
        // The "Handshake"
        int new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        
        if (new_socket < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue; // Don't crash, just wait for next
        }

        std::cout << "client connected!" << std::endl;
        // NEW: Launch a separate thread for this client
        std::thread client_thread(&Server::HandleClient, this, new_socket);
        client_thread.detach(); //allows the thread to run independently in the background
    }
}

/* replaced by AppendToLog()
void Server::Save() {
    std::ofstream outFile("bitstore.db");
    if (outFile.is_open()) {
        for (const auto& pair : kv_store) {
            // Format: key expiry_ms value
            outFile << pair.first << " " << pair.second.expiry_ms << " " << pair.second.value << "\n";
        }
        outFile.close();
    }
}
*/
// --- The Fast Writer ---
void Server::AppendToLog(const std::string& command_str) {
    if (aof_file.is_open()) {
        aof_file << command_str << "\n";
        aof_file.flush(); // Force the OS to write to disk immediately (Durability)
    }
}

void Server::CompactAOF() {
    // 1. Close the current open log
    if (aof_file.is_open()) {
        aof_file.close();
    }

    // 2. Open a temporary file
    std::ofstream tmp_file("bitstore.tmp");
    int keys_saved = 0;

    // 3. Dump the current state of memory into the temp file
    long long now = GetCurrentTimeMs();
    for (const auto& pair : kv_store) {
        const std::string& key = pair.first;
        const StoreItem& item = pair.second;

        // Only save keys that haven't expired yet
        if (item.expiry_ms == 0) {
            tmp_file << "SET " << key << " " << item.value << "\n";
            keys_saved++;
        } else if (item.expiry_ms > now) {
            // Calculate how many seconds are left
            int seconds_left = (item.expiry_ms - now) / 1000;
            if (seconds_left > 0) {
                tmp_file << "SETEX " << key << " " << seconds_left << " " << item.value << "\n";
                keys_saved++;
            }
        }
    }
    tmp_file.close();

    // 4. Swap the files (Delete old, rename new)
    std::remove("bitstore.aof");
    std::rename("bitstore.tmp", "bitstore.aof");

    // 5. Reopen the new, compacted log in Append mode
    aof_file.open("bitstore.aof", std::ios::app);
    
    std::cout << "[AOF] Compaction complete. Shrunk log to " << keys_saved << " keys." << std::endl;
}

// --- The Replayer ---
void Server::Load() {
    std::ifstream inFile("bitstore.aof");
    if (!inFile.is_open()) {
        std::cout << "[AOF] No existing log found. Starting fresh." << std::endl;
        return;
    }

    std::string line;
    int keys_restored = 0;

    // Read the diary line by line
    while (std::getline(inFile, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string command, key, value;
        ss >> command;

        // Replay the commands to rebuild the Hash Map
        if (command == "SET") {
            ss >> key;
            std::getline(ss >> std::ws, value);
            kv_store[key] = {value, 0};
            keys_restored++;
        } else if (command == "SETEX") {
            int seconds;
            ss >> key >> seconds;
            std::getline(ss >> std::ws, value);
            long long expiry = GetCurrentTimeMs() + (seconds * 1000LL);
            kv_store[key] = {value, expiry};
            keys_restored++;
        } else if (command == "DEL") {
            ss >> key;
            if (kv_store.erase(key)) {
                keys_restored--; // Adjust count if we deleted something
            }
        } else if (command == "LPUSH") {
            ss >> key;
            std::getline(ss >> std::ws, value);
            list_store[key].push_front(value);
            keys_restored++;
        } else if (command == "RPOP") {
            ss >> key;
            if (list_store.find(key) != list_store.end() && !list_store[key].empty()) {
                list_store[key].pop_back();
                if (list_store[key].empty()) {
                    list_store.erase(key);
                }
            }
        }
    }
    inFile.close();
    std::cout << "[AOF] Replayed log. Database restored with " << keys_restored << " active keys." << std::endl;
}



void Server::HandleClient(int client_fd) {
    char buffer[1024] = {0};
    
    while (true)
    {
        memset(buffer, 0, 1024); // Clear buffer
        int valread = read(client_fd, buffer, 1024);
        // --- THE DISCONNECT LOGIC ---
        // If read returns 0, client closed connection
        if (valread <= 0) {
            // Before we close the socket, scrub this client from all Pub/Sub channels!
            { //aquire lock------------
                std::lock_guard<std::mutex> lock(db_mutex); // Lock the DB first
                
                // Loop through every channel
                for (auto& pair : channels) {
                    auto& subscribers = pair.second;
                    
                    // C++ trick to find and remove an item from a vector
                    subscribers.erase(
                        std::remove(subscribers.begin(), subscribers.end(), client_fd), 
                        subscribers.end()
                    );
                }
            } //release lock------------

            close(client_fd);
            std::cout << "Client " << client_fd << " disconnected and scrubbed." << std::endl;
            break; 
        }
        // ------------------------------
            
        // Convert buffer to string and trim newline characters
        std::string request(buffer);
        
        // Remove \n or \r at the end (common from nc/telnet)
        while (!request.empty() && (request.back() == '\n' || request.back() == '\r')) {
            request.pop_back();
        }

        if (request.empty()) continue;

        // Parse Command
        std::stringstream ss(request);
        std::string command, key, value;
        ss >> command;

        std::string response;

        // --- LOCKING STARTS HERE (CS-section)  ---
        // We use a scope {} to limit how long we hold the lock
        // --- LOCKING STARTS HERE ---
        {
            std::lock_guard<std::mutex> lock(db_mutex);

            // --- Standard SET (Lives forever) ---
            if (command == "SET") {
                ss >> key ;
                std::getline(ss >> std::ws, value); // <--- FIX: Read the rest of the line
                if (!key.empty() && !value.empty()) {
                    kv_store[key] = {value, 0}; // 0 = no expiry
                    AppendToLog("SET " + key + " " + value);
                    response = "OK\n";
                } else {
                    response = "ERROR: SET key value\n";
                }
            }
            // --- NEW: SETEX (Set with Expiration) ---
            else if (command == "SETEX") {
                int seconds;
                ss >> key >> seconds;
                std::getline(ss >> std::ws, value); // <--- FIX: Read the rest of the line
                if (!key.empty() && !value.empty() && seconds > 0) {
                    long long expiry = GetCurrentTimeMs() + (seconds * 1000LL);
                    kv_store[key] = {value, expiry};
                    AppendToLog("SETEX " + key + " " + std::to_string(seconds) + " " + value);
                    response = "OK\n";
                } else {
                    response = "ERROR: SETEX key seconds value\n";
                }
            }
            // --- UPDATED GET (Lazy Expiration) ---
            else if (command == "GET") {
                ss >> key;
                auto it = kv_store.find(key);
                
                if (it != kv_store.end()) {
                    long long now = GetCurrentTimeMs();
                    // Check if it has an expiry AND if it is in the past
                    if (it->second.expiry_ms > 0 && now > it->second.expiry_ms) {
                        // It expired! Delete it and pretend it doesn't exist
                        kv_store.erase(it);
                        response = "(nil)\n";
                    } else {
                        // It's still valid
                        response = it->second.value + "\n";
                    }
                } else {
                    response = "(nil)\n";
                }
            }

            // --- NEW: DEL Command ---
            else if (command == "DEL") {
                ss >> key;
                if (!key.empty()) {
                    // .erase() returns the number of elements removed (1 or 0)
                    if (kv_store.erase(key)) {
                        AppendToLog("DEL " + key);
                        response = "(integer) 1\n"; // Standard Redis reply for successful delete
                    } else {
                        response = "(integer) 0\n"; // Key didn't exist
                    }
                } else {
                    response = "ERROR: DEL key\n";
                }
            }

            // --- NEW: KEYS Command ---
            else if (command == "KEYS") {
                response = "";
                for (const auto& pair : kv_store) {
                    response += pair.first + "\n"; //順序是依照hash bucket從hash value 0到N-1
                }
                if (response.empty()) {
                    response = "(empty list)\n";
                }
            }
            // --- NEW: COMPACT Command ---
            else if (command == "COMPACT") {
                CompactAOF();
                response = "OK: AOF Compacted\n";
            }

            // --- NEW: LPUSH (Left Push) ---
            else if (command == "LPUSH") {
                ss >> key;
                std::getline(ss >> std::ws, value);

                if (!key.empty() && !value.empty()) {
                    // push_front adds it to the left side
                    list_store[key].push_front(value);
                    AppendToLog("LPUSH " + key + " " + value);
                    
                    // Return the new length of the list
                    response = "(integer) " + std::to_string(list_store[key].size()) + "\n";
                } else {
                    response = "ERROR: LPUSH key value\n";
                }
            }
            // --- NEW: RPOP (Right Pop) ---
            else if (command == "RPOP") {
                ss >> key;
                
                // Check if the list exists and isn't empty
                if (list_store.find(key) != list_store.end() && !list_store[key].empty()) {
                    // Grab the item on the right
                    response = list_store[key].back() + "\n";
                    // Remove it from the right
                    list_store[key].pop_back();
                    
                    AppendToLog("RPOP " + key);

                    // Clean up: If the list is now empty, delete the key entirely
                    if (list_store[key].empty()) {
                        list_store.erase(key);
                    } 
                } else{
                    response = "(nil)\n";
                }
                
            }

            // --- NEW: SUBSCRIBE ---
            else if (command == "SUBSCRIBE") {
                ss >> key; // We use 'key' as the channel name here
                if (!key.empty()) {
                    // Add this client's socket ID to the channel's listener list
                    channels[key].push_back(client_fd);
                    response = "Subscribed to channel: " + key + "\n";
                } else {
                    response = "ERROR: SUBSCRIBE channel_name\n";
                }
            }
            // --- NEW: PUBLISH ---
            else if (command == "PUBLISH") {
                ss >> key; // The channel name
                std::getline(ss >> std::ws, value); // The message

                if (!key.empty() && !value.empty()) {
                    int listeners = 0;
                    // Check if the channel exists in our map
                    if (channels.find(key) != channels.end()) {
                        std::string broadcast_msg = "[MESSAGE - " + key + "] " + value + "\n";
                        
                        // Loop through every connected client in this channel and blast the message
                        for (int subscriber_fd : channels[key]) {
                            // Note: If a client disconnected without telling us, this send might fail, 
                            // but for our prototype, this is perfect.
                            send(subscriber_fd, broadcast_msg.c_str(), broadcast_msg.length(), 0);
                            listeners++;
                        }
                    }
                    response = "(integer) " + std::to_string(listeners) + " listeners received message\n";
                } else {
                    response = "ERROR: PUBLISH channel message\n";
                }
            }
            
            else {
                response = "ERROR: Unknown command\n";
            }
        } 
        // --- LOCK RELEASED AUTOMATICALLY HERE ---

        // Send response back to client
        send(client_fd, response.c_str(), response.length(), 0);
    }
}

void Server::GarbageCollector() {
    while (true) {
        // Sleep for 5 seconds between sweeps to save CPU
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        int deleted_count = 0;
        long long now = GetCurrentTimeMs();

        // --- LOCK THE DATABASE ---------------------------
        {
            std::lock_guard<std::mutex> lock(db_mutex);

            // Sweep through the entire map
            for (auto it = kv_store.begin(); it != kv_store.end(); ) {
                // If it has an expiry AND it is in the past
                if (it->second.expiry_ms > 0 && now > it->second.expiry_ms) {
                    std::string expired_key = it->first;// Grab the key name BEFORE erasing it
                    it = kv_store.erase(it); // Safely delete and move to the next item
                    deleted_count++;
                    // NEW: Write a DEL command to the log so it doesn't come back on restart
                    AppendToLog("DEL " + expired_key);
                } else {
                    ++it; // Move to the next item
                }
            }

            // If we deleted anything, save the new state to disk
            if (deleted_count > 0) {
                std::cout << "[GC] Cleaned up " << deleted_count << " expired keys." << std::endl;
            }
        }
        // --- LOCK RELEASED -----------------------------
    }
}