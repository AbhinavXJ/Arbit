#ifndef REAL_CROSS_ASSET_ARBITRAGE_HPP
#define REAL_CROSS_ASSET_ARBITRAGE_HPP

#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <mutex>

struct AssetPricePoint {
    double price;
    std::chrono::steady_clock::time_point timestamp;
};

struct CrossAssetRatio {
    double btc_price;
    double eth_price;
    double ratio;
    std::chrono::steady_clock::time_point timestamp;
};

struct RealCrossAssetOpportunity {
    std::string opportunity_type;
    std::string primary_exchange;
    std::string secondary_exchange;
    std::string asset_pair;
    double primary_ratio;
    double secondary_ratio;
    double ratio_spread_percent;
    double primary_btc_price;
    double primary_eth_price;
    double secondary_btc_price;
    double secondary_eth_price;
    std::string strategy;
    double expected_profit;
    double confidence;
    bool is_executable;
};

class RealCrossAssetArbitrage {
private:
    std::map<std::string, std::vector<AssetPricePoint>> asset_price_history;
    std::map<std::string, std::vector<CrossAssetRatio>> ratio_history;
    std::map<std::string, double> current_prices;
    std::mutex cross_asset_mutex;
    
    double min_ratio_spread_percent = 0.01;  
    double max_ratio_spread_percent = 2.0;   
    int history_window = 20;
    
    std::string generate_price_key(const std::string& exchange, const std::string& asset);
    std::string generate_ratio_key(const std::string& exchange);
    double calculate_ratio_fair_value(const std::vector<CrossAssetRatio>& history);
    double calculate_ratio_volatility(const std::vector<CrossAssetRatio>& history);
    
public:
    RealCrossAssetArbitrage();
    void update_asset_price(const std::string& exchange, const std::string& asset, double price);
    std::vector<RealCrossAssetOpportunity> scan_real_cross_asset_opportunities();
    void print_real_cross_asset_analysis();
};

#endif
