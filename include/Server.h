#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono> // For reading the system clock
#include <fstream> // for ofstream
#include <deque> // Double-Ended Que
#include <vector> //For our list of listeners
// Our custom data container
struct StoreItem {
    std::string value;
    long long expiry_ms; // 0 means it lives forever
};

class Server {
public:
    // Constructor: We tell it which port to listen on
    Server(int port);
    ~Server(); // <--- NEW: A Destructor to safely close our file when the server shuts down
    // The main loop: Bind to port, Listen, and Accept connections
    void Run();

private:
    int port;
    int server_fd; // File Descriptor for the server socket
    

    // Helper to handle a single client
    void HandleClient(int client_fd);
    // The Background Sweeper
    void GarbageCollector();

    //A map to store our data, 並可記錄expired time
    std::unordered_map<std::string, StoreItem> kv_store;
    // The List Store
    std::unordered_map<std::string, std::deque<std::string>> list_store;
    // NEW: The Pub/Sub Tracker
    // Maps "channel_name" -> [client_fd_1, client_fd_2, ...]
    std::unordered_map<std::string, std::vector<int>> channels;

    // Our persistent log file stream
    std::ofstream aof_file; 

    // REPLACED Save() with AppendToLog()
    void AppendToLog(const std::string& command_str);
    // NEW: The File Shrinker
    void CompactAOF();

    // Persistence helpers
    void Load();

    // The Lock
    std::mutex db_mutex;
};