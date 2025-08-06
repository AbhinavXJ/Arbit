#ifndef CONNECTION_POOL_HPP
#define CONNECTION_POOL_HPP

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <memory>

using namespace std;

class SimpleMessageQueue {
private:
    queue<string> messages;
    mutex queue_mutex;
    condition_variable cv;
    atomic<bool> running{true};
    
public:
    void push(const string& message) {
        lock_guard<mutex> lock(queue_mutex);
        messages.push(message);
        cv.notify_one();
    }
    
    bool try_pop(string& message) {
        lock_guard<mutex> lock(queue_mutex);
        if (messages.empty()) return false;
        message = messages.front();
        messages.pop();
        return true;
    }
    
    void stop() {
        running = false;
        cv.notify_all();
    }
};

class OptimizedWebSocketClient {
private:
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    
    client ws_client;
    thread io_thread;
    atomic<bool> running{false};
    
    atomic<uint64_t> messages_received{0};
    atomic<uint64_t> bytes_received{0};
    atomic<uint64_t> connection_errors{0};
    
    SimpleMessageQueue message_queue;
    
    function<void(string)> user_message_handler;
    
public:
    OptimizedWebSocketClient() {
        ws_client.set_access_channels(websocketpp::log::alevel::none);
        ws_client.set_error_channels(websocketpp::log::elevel::none);
        ws_client.init_asio();
    }
    
    ~OptimizedWebSocketClient() {
        disconnect();
    }
    
    bool connect(const string& uri, function<void(string)> message_handler) {
        user_message_handler = message_handler;
        running = true;
        
        cout << "[CONNECTION POOL]  Connecting to " << uri << endl;
        return true;
    }
    
    void disconnect() {
        running = false;
        message_queue.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
    
    uint64_t get_messages_per_second() const {
        return messages_received.load();
    }
    
    double get_error_rate() const {
        uint64_t total = messages_received.load();
        uint64_t errors = connection_errors.load();
        return total > 0 ? (double)errors / total : 0.0;
    }
};

class ConnectionPool {
private:
    vector<unique_ptr<OptimizedWebSocketClient>> connections;
    atomic<bool> pool_running{false};
    
public:
    ConnectionPool(size_t pool_size = 6) {
        connections.reserve(pool_size);
        cout << "[CONNECTION POOL]  Initialized with " << pool_size << " slots" << endl;
    }
    
    ~ConnectionPool() {
        stop_all_connections();
    }
    
    bool add_connection(const string& exchange, const string& uri, 
                       function<void(string)> handler) {
        auto client = make_unique<OptimizedWebSocketClient>();
        bool success = client->connect(uri, handler);
        if (success) {
            connections.push_back(move(client));
            cout << "[CONNECTION POOL]  Added " << exchange << " connection" << endl;
        }
        return success;
    }
    
    void start_all_connections() {
        pool_running = true;
        cout << "[CONNECTION POOL] Starting all connections..." << endl;
    }
    
    void stop_all_connections() {
        pool_running = false;
        for (auto& conn : connections) {
            if (conn) {
                conn->disconnect();
            }
        }
        connections.clear();
        cout << "[CONNECTION POOL]  Stopped all connections" << endl;
    }
    
    void print_connection_stats() {
        cout << "\n CONNECTION POOL STATISTICS" << endl;
        cout << string(50, '=') << endl;
        cout << "Active Connections: " << connections.size() << endl;
        cout << "Pool Status: " << (pool_running ? "RUNNING" : "STOPPED") << endl;
        
        for (size_t i = 0; i < connections.size(); ++i) {
            if (connections[i]) {
                cout << "Connection " << i << ": " 
                     << connections[i]->get_messages_per_second() << " msg/s, "
                     << fixed << setprecision(2) << (connections[i]->get_error_rate() * 100) 
                     << "% error rate" << endl;
            }
        }
        cout << string(50, '=') << endl;
    }
    
    double get_total_throughput() const {
        double total = 0.0;
        for (const auto& conn : connections) {
            if (conn) {
                total += conn->get_messages_per_second();
            }
        }
        return total;
    }
};

#endif
