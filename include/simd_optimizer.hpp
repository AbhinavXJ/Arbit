#ifndef SIMD_OPTIMIZER_HPP
#define SIMD_OPTIMIZER_HPP

#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <iomanip>
#include <chrono>

#ifdef __AVX2__
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

using namespace std;

class SIMDOptimizer {
private:
    static constexpr size_t VECTOR_SIZE = 8;  
    static constexpr size_t ALIGNMENT = 32;   
    
public:
    struct ArbitrageResult {
        array<float, VECTOR_SIZE> profits;
        array<float, VECTOR_SIZE> roi_percentages;
        array<uint32_t, VECTOR_SIZE> opportunity_flags;
        size_t valid_count;
        
        ArbitrageResult() : valid_count(0) {
            profits.fill(0.0f);
            roi_percentages.fill(0.0f);
            opportunity_flags.fill(0);
        }
    };
    
    static ArbitrageResult calculate_arbitrage_batch_simd(
        const vector<float>& prices_a,
        const vector<float>& prices_b,
        float min_profit_threshold
    );
    
    static void calculate_spreads_simd(
        const vector<float>& bids,
        const vector<float>& asks,
        vector<float>& spreads
    );
    
    static void calculate_percentages_simd(
        const vector<float>& numerators,
        const vector<float>& denominators,
        vector<float>& results
    );
    
    static void calculate_synthetic_prices_simd(
        const vector<float>& spot_prices,
        const vector<float>& time_to_expiry,
        float risk_free_rate,
        vector<float>& synthetic_futures
    );
    
    static float calculate_mean_simd(const vector<float>& values);
    static float calculate_std_dev_simd(const vector<float>& values, float mean);
    static pair<float, float> find_min_max_simd(const vector<float>& values);
    
    static void benchmark_simd_performance();
    static void print_simd_capabilities();
    
    static ArbitrageResult calculate_arbitrage_batch_scalar(
        const vector<float>& prices_a,
        const vector<float>& prices_b,
        float min_profit_threshold
    );
};

template<typename T>
class AlignedVector {
private:
    T* data_;
    size_t size_;
    size_t capacity_;
    
public:
    AlignedVector(size_t initial_capacity = 1024) 
        : size_(0), capacity_(initial_capacity) {
#ifdef _WIN32
        data_ = static_cast<T*>(_aligned_malloc(capacity_ * sizeof(T), 32));
#else
        data_ = static_cast<T*>(aligned_alloc(32, capacity_ * sizeof(T)));
#endif
    }
    
    ~AlignedVector() {
        if (data_) {
#ifdef _WIN32
            _aligned_free(data_);
#else
            free(data_);
#endif
        }
    }
    
    void push_back(const T& value) {
        if (size_ >= capacity_) resize(capacity_ * 2);
        data_[size_++] = value;
    }
    
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    
    T* data() { return data_; }
    const T* data() const { return data_; }
    size_t size() const { return size_; }
    
private:
    void resize(size_t new_capacity) {
#ifdef _WIN32
        T* new_data = static_cast<T*>(_aligned_malloc(new_capacity * sizeof(T), 32));
#else
        T* new_data = static_cast<T*>(aligned_alloc(32, new_capacity * sizeof(T)));
#endif
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = data_[i];
        }
#ifdef _WIN32
        _aligned_free(data_);
#else
        free(data_);
#endif
        data_ = new_data;
        capacity_ = new_capacity;
    }
};

#endif
