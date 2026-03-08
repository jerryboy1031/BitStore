# 🚀 BitStore

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Status](https://img.shields.io/badge/Status-Active_Development-orange.svg)

**BitStore** is a lightweight, high-performance In-Memory Key-Value Store(cache) and message broker built entirely from scratch in C++. 

**Concept**: A simplified version of Redis. This server listens on a TCP port, accepts commands (like SET key value or GET key), stores data in memory, and returns responses.

**Goal**: I want something like Redis (Remote Dictionary Server) 

---

## 🎥 Demo

待補上

---

## ✨ Core Features

* **Blazing Fast In-Memory Store:** Achieves $O(1)$ read/write lookups using standard C++ hash maps.
* **Time-To-Live (TTL) & Garbage Collection:** Assign expiration times to keys. A dedicated background `std::thread` safely sweeps and reclaims memory without blocking client requests.
* **Data Durability (AOF):** Uses an Append-Only File (AOF) log to persist data to disk instantly. Includes a `COMPACT` engine to safely shrink the log file in the background.
* **Task Queues:** Native support for double-ended queues (`std::deque`), perfect for producer/consumer workloads.
* 📢 **Real-Time Pub/Sub:** A 1-to-many message broadcasting system with robust socket management and OS-level `SIGPIPE` armoring to prevent crashes from dropped clients.
* **Custom REPL Client:** Includes `bitstore-cli`, a dedicated, interactive command-line interface for seamless database interaction.

---

## 🏗️ Architecture



BitStore separates concerns into distinct layers:
1. **Network Layer:** Raw TCP sockets handling concurrent client connections.
2. **Execution Layer:** Parses commands and manages thread-safe access to memory via `std::mutex`.
3. **Storage Layer:** Manages the `std::unordered_map` for strings, `std::deque` for lists, and the Pub/Sub routing tables.
4. **Persistence Layer:** Streams state changes to the disk via `std::ofstream`.

---

## 🚀 Getting Started

### Prerequisites
* A C++17 compatible compiler (e.g., `g++` or `clang++`)
* Linux or macOS (POSIX-compliant OS for socket programming)

### Build Instructions

1. **Clone the repository:**
   ```  
   git clone [https://github.com/YourUsername/BitStore.git](https://github.com/YourUsername/BitStore.git)
   cd BitStore
   ```
2. **Compile the Server and Client:**
    ```bash
    # Compile the server (requires pthread for background threads)
    g++ src/Server.cpp src/main.cpp -o BitStore -pthread
    # Compile the CLI tool
    g++ src/bitstore-cli.cpp -o bitstore-cli
    ```
3. **Run the Database:**
    ```bash
    # Open a terminal and start the server:
    ./BitStore
    ```
4. **Connect via CLI:**
    ```bash
    # Open a second terminal and launch the client:
    ./bitstore-cli
    ```

## 🛠️ Supported Commands

### 📝 Strings & Keys
| Command | Usage | Description |
| :--- | :--- | :--- |
| `SET` | `SET key value` | Stores a string value. |
| `GET` | `GET key` | Retrieves a string value. Returns `(nil)` if not found. |
| `SETEX` | `SETEX key seconds value` | Stores a key that will self-destruct after X seconds. |
| `DEL` | `DEL key` | Manually deletes a key from memory. |
| `KEYS` | `KEYS` | Lists all active, non-expired keys in the database. |

### 📦 Queues (Lists)
| Command | Usage | Description |
| :--- | :--- | :--- |
| `LPUSH` | `LPUSH list value` | Pushes a value to the left side of a queue. |
| `RPOP` | `RPOP list` | Pops and returns the oldest value from the right side. |

### 📢 Pub/Sub
| Command | Usage | Description |
| :--- | :--- | :--- |
| `SUBSCRIBE`| `SUBSCRIBE channel` | Listens to a channel for real-time messages. |
| `PUBLISH` | `PUBLISH channel msg` | Broadcasts a message to all listeners of the channel. |

### 💾 Server Management
| Command | Usage | Description |
| :--- | :--- | :--- |
| `COMPACT` | `COMPACT` | Re-writes the AOF log to save disk space. |
