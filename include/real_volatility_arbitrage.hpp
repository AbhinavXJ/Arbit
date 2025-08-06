#ifndef REAL_VOLATILITY_ARBITRAGE_HPP
#define REAL_VOLATILITY_ARBITRAGE_HPP

#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <mutex>

struct MarketDataPoint {
    double spot_price;
    double futures_price;
    double basis_bps;
    std::chrono::steady_clock::time_point timestamp;
};

struct VolatilityIndicator {
    double realized_volatility;
    double basis_implied_volatility;
    double volatility_risk_premium;
    double confidence_score;
    std::chrono::steady_clock::time_point timestamp;
};

struct RealVolatilityOpportunity {
    std::string exchange;
    std::string asset;
    double current_spot_price;
    double current_futures_price;
    double basis_bps;
    double realized_vol;
    double implied_vol_proxy;
    double volatility_spread;
    std::string strategy;
    double expected_profit;
    double confidence;
    bool is_executable;
};

class RealVolatilityArbitrage {
private:
    std::map<std::string, std::vector<MarketDataPoint>> market_history;
    std::map<std::string, VolatilityIndicator> current_indicators;
    std::mutex volatility_mutex;
    
    double min_vol_spread_bps = 20;   
    double max_vol_spread_bps = 500; 
    int history_window = 30;         
    
    std::string generate_key(const std::string& exchange, const std::string& asset);
    double calculate_realized_volatility(const std::vector<MarketDataPoint>& history);
    double calculate_basis_implied_volatility(const std::vector<MarketDataPoint>& history);
    double calculate_volatility_risk_premium(double realized_vol, double implied_vol);
    
public:
    RealVolatilityArbitrage();
    void update_market_data(const std::string& exchange, const std::string& asset, 
                           double spot_price, double futures_price);
    std::vector<RealVolatilityOpportunity> scan_real_volatility_opportunities();
    void print_real_volatility_analysis();
};

#endif
