#include "real_cross_asset_arbitrage.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace std;

RealCrossAssetArbitrage::RealCrossAssetArbitrage() {
    cout << "[REAL CROSS-ASSET] Engine initialized - NO simulation, real ratios only" << endl;
    cout << "[CROSS-ASSET] FIXED Threshold: ±" << min_ratio_spread_percent << "% (LOWERED for sensitivity)" << endl;
}

string RealCrossAssetArbitrage::generate_price_key(const string& exchange, const string& asset) {
    return exchange + "_" + asset;
}

string RealCrossAssetArbitrage::generate_ratio_key(const string& exchange) {
    return exchange + "_BTC_ETH_ratio";
}

void RealCrossAssetArbitrage::update_asset_price(const string& exchange, const string& asset, double price) {
    lock_guard<mutex> lock(cross_asset_mutex);
    
    if (price <= 0) return;
    
    string price_key = generate_price_key(exchange, asset);
    current_prices[price_key] = price;
    
  
    AssetPricePoint point;
    point.price = price;
    point.timestamp = chrono::steady_clock::now();
    
    auto& history = asset_price_history[price_key];
    history.push_back(point);
    
    if (history.size() > static_cast<size_t>(history_window)) {
        history.erase(history.begin());
    }
    
    string btc_key = exchange + "_Bitcoin";
    string eth_key = exchange + "_Ethereum";
    
    if (current_prices.find(btc_key) != current_prices.end() &&
        current_prices.find(eth_key) != current_prices.end()) {
        
        double btc_price = current_prices[btc_key];
        double eth_price = current_prices[eth_key];
        
        if (eth_price > 0) {
            double ratio = btc_price / eth_price;
            
            CrossAssetRatio ratio_point;
            ratio_point.btc_price = btc_price;
            ratio_point.eth_price = eth_price;
            ratio_point.ratio = ratio;
            ratio_point.timestamp = chrono::steady_clock::now();
            
            string ratio_key = generate_ratio_key(exchange);
            auto& ratio_hist = ratio_history[ratio_key];
            ratio_hist.push_back(ratio_point);
            
            if (ratio_hist.size() > static_cast<size_t>(history_window)) {
                ratio_hist.erase(ratio_hist.begin());
            }
            
         
        }
    }
}

double RealCrossAssetArbitrage::calculate_ratio_fair_value(const vector<CrossAssetRatio>& history) {
    if (history.size() < 2) return 0.0; // REDUCED from 3 to 2
    
    double sum = 0.0;
    double weight_sum = 0.0;
    double alpha = 0.2; // Decay factor
    
    for (size_t i = 0; i < history.size(); ++i) {
        double weight = pow(alpha, history.size() - 1 - i);
        sum += history[i].ratio * weight;
        weight_sum += weight;
    }
    
    return weight_sum > 0 ? sum / weight_sum : 0.0;
}

double RealCrossAssetArbitrage::calculate_ratio_volatility(const vector<CrossAssetRatio>& history) {
    if (history.size() < 3) return 0.0;
    
    vector<double> ratio_returns;
    for (size_t i = 1; i < history.size(); i++) {
        if (history[i-1].ratio > 0) {
            double return_rate = log(history[i].ratio / history[i-1].ratio);
            ratio_returns.push_back(return_rate);
        }
    }
    
    if (ratio_returns.empty()) return 0.0;
    
    double mean = accumulate(ratio_returns.begin(), ratio_returns.end(), 0.0) / ratio_returns.size();
    double variance = 0.0;
    for (double ret : ratio_returns) {
        variance += pow(ret - mean, 2);
    }
    variance /= (ratio_returns.size() - 1);
    
    return sqrt(variance) * sqrt(525600) * 100; // Annualized %
}

vector<RealCrossAssetOpportunity> RealCrossAssetArbitrage::scan_real_cross_asset_opportunities() {
    lock_guard<mutex> lock(cross_asset_mutex);
    vector<RealCrossAssetOpportunity> opportunities;
    
    
    map<string, double> current_ratios;
    map<string, pair<double, double>> current_asset_prices; // BTC, ETH
    
    vector<string> exchanges = {"Binance", "Bybit", "OKX"};
    
    for (const string& exchange : exchanges) {
        string btc_key = exchange + "_Bitcoin";
        string eth_key = exchange + "_Ethereum";
        
        if (current_prices.find(btc_key) != current_prices.end() &&
            current_prices.find(eth_key) != current_prices.end()) {
            
            double btc_price = current_prices[btc_key];
            double eth_price = current_prices[eth_key];
            
            if (eth_price > 0) {
                double ratio = btc_price / eth_price;
                current_ratios[exchange] = ratio;
                current_asset_prices[exchange] = make_pair(btc_price, eth_price);
                
                
            }
        }
    }
    
    if (current_ratios.size() >= 2) {
        for (auto it1 = current_ratios.begin(); it1 != current_ratios.end(); ++it1) {
            for (auto it2 = next(it1); it2 != current_ratios.end(); ++it2) {
                const string& exchange1 = it1->first;
                const string& exchange2 = it2->first;
                double ratio1 = it1->second;
                double ratio2 = it2->second;
                
                double ratio_spread_percent = abs(ratio1 - ratio2) / ratio2 * 100.0;
                
             
                
                if (ratio_spread_percent >= min_ratio_spread_percent && 
                    ratio_spread_percent <= max_ratio_spread_percent) {
                 
                    
                    RealCrossAssetOpportunity opp;
                    opp.opportunity_type = "Cross_Exchange_BTC_ETH_Ratio";
                    opp.primary_exchange = (ratio1 < ratio2) ? exchange1 : exchange2;
                    opp.secondary_exchange = (ratio1 < ratio2) ? exchange2 : exchange1;
                    opp.asset_pair = "BTC/ETH";
                    opp.primary_ratio = min(ratio1, ratio2);
                    opp.secondary_ratio = max(ratio1, ratio2);
                    opp.ratio_spread_percent = ratio_spread_percent;
                    
                    auto& primary_prices = current_asset_prices[opp.primary_exchange];
                    auto& secondary_prices = current_asset_prices[opp.secondary_exchange];
                    
                    opp.primary_btc_price = primary_prices.first;
                    opp.primary_eth_price = primary_prices.second;
                    opp.secondary_btc_price = secondary_prices.first;
                    opp.secondary_eth_price = secondary_prices.second;
                    
                    opp.strategy = "Buy BTC/" + opp.primary_exchange + " + Sell ETH/" + opp.primary_exchange + 
                                  " | Sell BTC/" + opp.secondary_exchange + " + Buy ETH/" + opp.secondary_exchange;
                    
                    opp.expected_profit = ratio_spread_percent * min(opp.primary_btc_price, opp.secondary_btc_price) * 0.01;
                    opp.confidence = min(0.8, 0.5 + (ratio_spread_percent / 0.2));
                    opp.is_executable = ratio_spread_percent >= min_ratio_spread_percent;
                    
                    opportunities.push_back(opp);
                }
            }
        }
    }
    
    for (const auto& entry : ratio_history) {
        const string& ratio_key = entry.first;
        const vector<CrossAssetRatio>& history = entry.second;
        
        if (history.size() < 2) continue; 
        
        string exchange = ratio_key.substr(0, ratio_key.find('_'));
        double current_ratio = history.back().ratio;
        double fair_value = calculate_ratio_fair_value(history);
        
        if (fair_value > 0) {
            double deviation_percent = abs((current_ratio - fair_value) / fair_value) * 100.0;
            
          
            
            if (deviation_percent >= min_ratio_spread_percent) {
   
                
                RealCrossAssetOpportunity opp;
                opp.opportunity_type = "Ratio_Mean_Reversion";
                opp.primary_exchange = exchange;
                opp.secondary_exchange = exchange;
                opp.asset_pair = "BTC/ETH";
                opp.primary_ratio = current_ratio;
                opp.secondary_ratio = fair_value;
                opp.ratio_spread_percent = deviation_percent;
                
                opp.primary_btc_price = history.back().btc_price;
                opp.primary_eth_price = history.back().eth_price;
                opp.secondary_btc_price = history.back().btc_price;
                opp.secondary_eth_price = history.back().eth_price;
                
                if (current_ratio > fair_value) {
                    opp.strategy = "Short BTC/Long ETH on " + exchange + " (ratio overextended)";
                } else {
                    opp.strategy = "Long BTC/Short ETH on " + exchange + " (ratio oversold)";
                }
                
                opp.expected_profit = deviation_percent * opp.primary_btc_price * 0.005;
                opp.confidence = min(0.85, 0.4 + (deviation_percent / 0.1));
                opp.is_executable = deviation_percent >= min_ratio_spread_percent;
                
                opportunities.push_back(opp);
            }
        }
    }
    
    
    sort(opportunities.begin(), opportunities.end(),
        [](const RealCrossAssetOpportunity& a, const RealCrossAssetOpportunity& b) {
            return a.ratio_spread_percent > b.ratio_spread_percent;
        });
    
    return opportunities;
}

void RealCrossAssetArbitrage::print_real_cross_asset_analysis() {
    auto opportunities = scan_real_cross_asset_opportunities();
    
    cout << "\n REAL CROSS-ASSET ARBITRAGE ANALYSIS" << endl;
    cout << string(100, '=') << endl;
    
    if (opportunities.empty()) {
        cout << " NO SIGNIFICANT CROSS-ASSET MISPRICINGS DETECTED" << endl;
        cout << " BTC/ETH ratios are aligned across exchanges" << endl;
        cout << " FIXED Monitoring threshold: ±" << fixed << setprecision(3) << min_ratio_spread_percent << "%" << endl;
        
        cout << "\n CURRENT BTC/ETH RATIOS:" << endl;
        vector<string> exchanges = {"Binance", "Bybit", "OKX"};
        for (const string& exchange : exchanges) {
            string btc_key = exchange + "_Bitcoin";
            string eth_key = exchange + "_Ethereum";
            
            if (current_prices.find(btc_key) != current_prices.end() &&
                current_prices.find(eth_key) != current_prices.end()) {
                
                double btc_price = current_prices[btc_key];
                double eth_price = current_prices[eth_key];
                
                if (eth_price > 0) {
                    double ratio = btc_price / eth_price;
                    cout << "  " << exchange << ": " << fixed << setprecision(3) << ratio 
                         << " (BTC: $" << fixed << setprecision(2) << btc_price 
                         << ", ETH: $" << eth_price << ")" << endl;
                }
            }
        }
    } else {
        cout << " " << opportunities.size() << " REAL CROSS-ASSET OPPORTUNITIES DETECTED:" << endl;
        cout << string(120, '-') << endl;
        cout << setw(25) << "Type" << setw(15) << "Exchange(s)"
             << setw(15) << "Asset Pair" << setw(12) << "Ratio 1"
             << setw(12) << "Ratio 2" << setw(12) << "Spread%"
             << setw(12) << "Profit $" << setw(12) << "Confidence" << endl;
        cout << string(120, '-') << endl;
        
        for (const auto& opp : opportunities) {
            string exchanges = (opp.primary_exchange == opp.secondary_exchange) ?
                opp.primary_exchange : opp.primary_exchange + "-" + opp.secondary_exchange;
            
            cout << setw(25) << opp.opportunity_type
                 << setw(15) << exchanges
                 << setw(15) << opp.asset_pair
                 << fixed << setprecision(3) << setw(12) << opp.primary_ratio
                 << fixed << setprecision(3) << setw(12) << opp.secondary_ratio
                 << fixed << setprecision(3) << setw(11) << opp.ratio_spread_percent << "%"
                 << " $" << fixed << setprecision(2) << setw(10) << opp.expected_profit
                 << fixed << setprecision(1) << setw(11) << (opp.confidence * 100) << "%" << endl;
        }
        
        cout << "\n REAL CROSS-ASSET TRADING STRATEGIES:" << endl;
        for (size_t i = 0; i < min(opportunities.size(), size_t(3)); i++) {
            const auto& opp = opportunities[i];
            cout << "\n[CROSS-ASSET " << (i+1) << "] " << opp.opportunity_type << endl;
            cout << " Strategy: " << opp.strategy << endl;
            cout << " Primary Ratio: " << fixed << setprecision(3) << opp.primary_ratio 
                 << " (" << opp.primary_exchange << ")" << endl;
            cout << " Secondary Ratio: " << fixed << setprecision(3) << opp.secondary_ratio 
                 << " (" << opp.secondary_exchange << ")" << endl;
            cout << " Spread: " << fixed << setprecision(3) << opp.ratio_spread_percent << "%" << endl;
            cout << " Expected Profit: $" << fixed << setprecision(2) << opp.expected_profit << endl;
            cout << " Confidence: " << fixed << setprecision(1) << (opp.confidence * 100) << "%" << endl;
            cout << " Prices: BTC $" << fixed << setprecision(2) << opp.primary_btc_price 
                 << " | ETH $" << opp.primary_eth_price << endl;
        }
    }
    cout << string(100, '=') << endl;
}
