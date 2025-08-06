#ifndef MULTI_LEG_ARBITRAGE_HPP
#define MULTI_LEG_ARBITRAGE_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>

struct ArbitrageLeg {
    std::string exchange;
    std::string instrument;
    std::string action;         
    double      quantity;
    double      price;
    std::string expiry;          
    std::string instrument_type; 
};

struct MultiLegStrategy {
    std::string                              strategy_id;
    std::string                              strategy_type;   
    std::vector<ArbitrageLeg>                legs;
    double                                   expected_profit; // USD
    double                                   roi_percent;     
    double                                   risk_score;      
    double                                   confidence;      
    bool                                     is_executable;
    std::chrono::steady_clock::time_point    timestamp;
};

struct RealMarketData {
    std::string                              exchange;
    std::string                              asset;          
    double                                   spot_price;
    double                                   futures_price;
    double                                   basis_bps;
    double                                   implied_volatility;
    std::chrono::steady_clock::time_point    timestamp;
};

class MultiLegArbitrageEngine {
private:
    std::map<std::string, RealMarketData> market_data;

    double calculate_implied_volatility_from_basis(double basis_bps,
                                                   double time_to_expiry);
    double estimate_option_premium(double spot,
                                   double strike,
                                   double vol,
                                   double time_to_expiry,
                                   bool   is_call);

    std::vector<MultiLegStrategy> find_calendar_spreads();
    std::vector<MultiLegStrategy> find_synthetic_replication();
    std::vector<MultiLegStrategy> find_real_butterfly_spreads();
    std::vector<MultiLegStrategy> scan_all_strategies();

public:
    MultiLegArbitrageEngine();
    void update_market_data(const std::string& exchange,
                            const std::string& asset,
                            double            spot_price,
                            double            futures_price);

    void print_multi_leg_opportunities();
};

#endif
