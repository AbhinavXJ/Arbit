#ifndef FAST_ORDERBOOK_HPP
#define FAST_ORDERBOOK_HPP

#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <string>        
#include <algorithm>     
#include <cfloat>        

#ifdef __SSE2__
#include <immintrin.h>
#endif

using namespace std;

template<size_t Size>
class LockFreeCircularBuffer {
private:
    alignas(64) array<atomic<double>, Size> prices;
    alignas(64) array<atomic<double>, Size> quantities;
    alignas(64) atomic<size_t> write_index{0};
    alignas(64) atomic<size_t> read_index{0};
    
public:
    bool try_push(double price, double quantity) {
        size_t current_write = write_index.load(memory_order_relaxed);
        size_t next_write = (current_write + 1) % Size;
        
        if (next_write == read_index.load(memory_order_acquire)) {
            return false; 
        }
        
        prices[current_write].store(price, memory_order_relaxed);
        quantities[current_write].store(quantity, memory_order_relaxed);
        write_index.store(next_write, memory_order_release);
        return true;
    }
    
    bool try_pop(double& price, double& quantity) {
        size_t current_read = read_index.load(memory_order_relaxed);
        if (current_read == write_index.load(memory_order_acquire)) {
            return false; 
        }
        
        price = prices[current_read].load(memory_order_relaxed);
        quantity = quantities[current_read].load(memory_order_relaxed);
        read_index.store((current_read + 1) % Size, memory_order_release);
        return true;
    }
    
    size_t size() const {
        size_t write = write_index.load(memory_order_relaxed);
        size_t read = read_index.load(memory_order_relaxed);
        return (write >= read) ? (write - read) : (Size - read + write);
    }
};

class FastOrderbook {
private:
    static constexpr size_t MAX_LEVELS = 1000;
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    alignas(CACHE_LINE_SIZE) array<double, MAX_LEVELS> bid_prices;
    alignas(CACHE_LINE_SIZE) array<double, MAX_LEVELS> bid_quantities;
    alignas(CACHE_LINE_SIZE) array<double, MAX_LEVELS> ask_prices;
    alignas(CACHE_LINE_SIZE) array<double, MAX_LEVELS> ask_quantities;
    
    atomic<size_t> bid_count{0};
    atomic<size_t> ask_count{0};
    
    static thread_local vector<char> string_buffer;
    
public:
    FastOrderbook() {
        fill(bid_prices.begin(), bid_prices.end(), 0.0);
        fill(ask_prices.begin(), ask_prices.end(), DBL_MAX);
    }
    
    bool insert_bid(double price, double quantity) {
        size_t count = bid_count.load(memory_order_relaxed);
        if (count >= MAX_LEVELS) return false;
        
        bid_prices[count] = price;
        bid_quantities[count] = quantity;
        bid_count.store(count + 1, memory_order_release);
        return true;
    }
    
    bool insert_ask(double price, double quantity) {
        size_t count = ask_count.load(memory_order_relaxed);
        if (count >= MAX_LEVELS) return false;
        
        ask_prices[count] = price;
        ask_quantities[count] = quantity;
        ask_count.store(count + 1, memory_order_release);
        return true;
    }
    
    pair<double, double> get_best_bid() const {
        size_t count = bid_count.load(memory_order_acquire);
        if (count == 0) return make_pair(0.0, 0.0);
        return make_pair(bid_prices[0], bid_quantities[0]);
    }
    
    pair<double, double> get_best_ask() const {
        size_t count = ask_count.load(memory_order_acquire);
        if (count == 0) return make_pair(0.0, 0.0);
        return make_pair(ask_prices[0], ask_quantities[0]);
    }
    
    double calculate_spread() const {
        auto best_bid = get_best_bid();
        auto best_ask = get_best_ask();
        if (best_bid.first == 0.0 || best_ask.first == 0.0) return 0.0;
        return best_ask.first - best_bid.first;
    }
    
    string serialize_top_levels(size_t levels) const {
        string result;
        result.reserve(levels * 50); 
        
        size_t bid_cnt = min(levels, bid_count.load(memory_order_acquire));
        size_t ask_cnt = min(levels, ask_count.load(memory_order_acquire));
        
        for (size_t i = 0; i < bid_cnt; ++i) {
            result += "B:" + to_string(bid_prices[i]) + "@" + to_string(bid_quantities[i]) + ";";
        }
        for (size_t i = 0; i < ask_cnt; ++i) {
            result += "A:" + to_string(ask_prices[i]) + "@" + to_string(ask_quantities[i]) + ";";
        }
        
        return result;
    }
    
    void clear() {
        bid_count.store(0, memory_order_relaxed);
        ask_count.store(0, memory_order_relaxed);
    }
    
    size_t get_bid_count() const {
        return bid_count.load(memory_order_acquire);
    }
    
    size_t get_ask_count() const {
        return ask_count.load(memory_order_acquire);
    }
};

thread_local vector<char> FastOrderbook::string_buffer;

#endif
