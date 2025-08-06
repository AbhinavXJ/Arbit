#ifndef SYNTHETIC_ENGINE_HPP
#define SYNTHETIC_ENGINE_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <cmath>
#include <vector>  

using namespace std;

struct SyntheticPrice {
    double real_price;
    double synthetic_price;
    double mispricing_percent;
    double funding_rate;
    string exchange;
    string instrument_type; 
    chrono::steady_clock::time_point timestamp;
    bool is_valid;
};

struct ConfigThresholds {
    double min_mispricing_percent = 0.01;  
    double max_mispricing_percent = 5.0;     
    double default_funding_rate = 0.0001;  
    double risk_free_rate = 0.05;          
    int calculation_interval_ms = 1000;     
    bool enable_logging = true;
};

class SyntheticEngine {
private:
    unordered_map<string, SyntheticPrice> synthetic_prices;
    mutable mutex synthetic_mutex;
    atomic<bool> running{true};
    ConfigThresholds config;
    thread calculation_thread;
    
    unordered_map<string, double> funding_rates;
    mutable mutex funding_mutex;
    
    double calculate_synthetic_spot_from_perp(double perp_price, double funding_rate);
    double calculate_synthetic_futures_price(double spot_price, double time_to_expiry, double rate);
    double calculate_mispricing_percent(double real_price, double synthetic_price);
    
    double get_current_funding_rate(const string& exchange, const string& symbol);
    double estimate_time_to_expiry(); 
    string generate_key(const string& exchange, const string& market);
    
    void calculation_loop();
    
public:
    SyntheticEngine(const ConfigThresholds& cfg = ConfigThresholds{});
    ~SyntheticEngine();
    
    void start();
    void stop();
    
    vector<SyntheticPrice> get_mispricing_opportunities();
    SyntheticPrice get_synthetic_price(const string& exchange, const string& market);
    
    void update_funding_rate(const string& exchange, const string& symbol, double rate);
    void set_config(const ConfigThresholds& cfg);
    
    void print_mispricing_opportunities();
    void log_synthetic_calculations();
};

extern SyntheticEngine* global_synthetic_engine;

#endif
