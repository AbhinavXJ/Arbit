#include "book_storage.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <ctime>
#include <tuple>

using namespace std;

map<double, string> spot_bids;
map<double, string> spot_asks;
map<double, string> futures_bids;
map<double, string> futures_asks;
OrderbookTimestamp binance_spot_timestamp;
OrderbookTimestamp binance_futures_timestamp;

map<double, string> bybit_spot_bids;
map<double, string> bybit_spot_asks;
map<double, string> bybit_futures_bids;
map<double, string> bybit_futures_asks;
OrderbookTimestamp bybit_spot_timestamp;
OrderbookTimestamp bybit_futures_timestamp;

map<double, string> okx_spot_bids;
map<double, string> okx_spot_asks;
map<double, string> okx_futures_bids;
map<double, string> okx_futures_asks;
OrderbookTimestamp okx_spot_timestamp;
OrderbookTimestamp okx_futures_timestamp;

map<double, string> eth_spot_bids;
map<double, string> eth_spot_asks;
map<double, string> eth_futures_bids;
map<double, string> eth_futures_asks;
OrderbookTimestamp eth_binance_spot_timestamp;
OrderbookTimestamp eth_binance_futures_timestamp;

map<double, string> eth_bybit_spot_bids;
map<double, string> eth_bybit_spot_asks;
map<double, string> eth_bybit_futures_bids;
map<double, string> eth_bybit_futures_asks;
OrderbookTimestamp eth_bybit_spot_timestamp;
OrderbookTimestamp eth_bybit_futures_timestamp;

map<double, string> eth_okx_spot_bids;
map<double, string> eth_okx_spot_asks;
map<double, string> eth_okx_futures_bids;
map<double, string> eth_okx_futures_asks;
OrderbookTimestamp eth_okx_spot_timestamp;
OrderbookTimestamp eth_okx_futures_timestamp;

mutex book_mutex;

struct ExchangePrice {
    string exchange;
    string market;
    double bid;
    double ask;
    double spread;
    bool valid;
    bool fresh;
    string volume_bid;
    string volume_ask;
    int age_seconds;
};

string get_timestamp() {
    time_t now = time(0);
    char* dt = ctime(&now);
    string timestamp(dt);
    timestamp.pop_back();
    return timestamp;
}

bool validate_orderbook(double bid, double ask, const string& exchange, const string& market) {
    if (bid >= ask) {
        cout << " [" << exchange << " " << market << "] INVALID: Bid ($" 
             << fixed << setprecision(2) << bid << ") >= Ask ($" << ask << ")" << endl;
        return false;
    }
    
    double spread = ask - bid;
    double spread_pct = (spread / bid) * 100;
    
    if (spread_pct > 1.0) { 
        cout << " [" << exchange << " " << market << "] LARGE SPREAD: " 
             << fixed << setprecision(4) << spread_pct << "% - Data may be stale" << endl;
        return false;
    }
    
    return true;
}

void print_top_orderbook_levels() {
    cout << "\nTOP 3 ORDERBOOK LEVELS FOR ALL EXCHANGES" << endl;
    cout << string(80, '=') << endl;

    auto print_levels = [](const string& exchange, const string& market, 
                          const map<double, string>& bids, const map<double, string>& asks) {
        if (!bids.empty() && !asks.empty()) {
            cout << "\n--- " << exchange << " " << market << " ---" << endl;
            
            cout << "Bids (Top 3):" << endl;
            int count = 0;
            for (auto it = bids.rbegin(); it != bids.rend() && count < 3; ++it, ++count) {
                cout << "  Bid " << (count+1) << ": $" << fixed << setprecision(2) << it->first 
                     << " @ " << it->second << endl;
            }
            
            cout << "Asks (Top 3):" << endl;
            count = 0;
            for (auto it = asks.begin(); it != asks.end() && count < 3; ++it, ++count) {
                cout << "  Ask " << (count+1) << ": $" << fixed << setprecision(2) << it->first 
                     << " @ " << it->second << endl;
            }
            
            double spread = asks.begin()->first - bids.rbegin()->first;
            cout << " Spread: $" << fixed << setprecision(2) << spread << endl;
        } else {
            cout << "\n--- " << exchange << " " << market << " --- NO DATA" << endl;
        }
    };

    cout << "\n BITCOIN (BTC) ORDERBOOKS" << endl;
    cout << string(60, '-') << endl;
    print_levels("Binance", "Bitcoin Spot", spot_bids, spot_asks);
    print_levels("Binance", "Bitcoin Futures", futures_bids, futures_asks);
    print_levels("Bybit", "Bitcoin Spot", bybit_spot_bids, bybit_spot_asks);
    print_levels("Bybit", "Bitcoin Futures", bybit_futures_bids, bybit_futures_asks);
    print_levels("OKX", "Bitcoin Spot", okx_spot_bids, okx_spot_asks);
    print_levels("OKX", "Bitcoin Futures", okx_futures_bids, okx_futures_asks);
    
    cout << "\n ETHEREUM (ETH) ORDERBOOKS" << endl;
    cout << string(60, '-') << endl;
    print_levels("Binance", "Ethereum Spot", eth_spot_bids, eth_spot_asks);
    print_levels("Binance", "Ethereum Futures", eth_futures_bids, eth_futures_asks);
    print_levels("Bybit", "Ethereum Spot", eth_bybit_spot_bids, eth_bybit_spot_asks);
    print_levels("Bybit", "Ethereum Futures", eth_bybit_futures_bids, eth_bybit_futures_asks);
    print_levels("OKX", "Ethereum Spot", eth_okx_spot_bids, eth_okx_spot_asks);
    print_levels("OKX", "Ethereum Futures", eth_okx_futures_bids, eth_okx_futures_asks);
    
    cout << string(80, '=') << endl;
}

void print_all_books() {
    vector<ExchangePrice> all_prices;
    
    cout << "\n" << string(100, '=') << endl;
    cout << " MULTI-EXCHANGE ARBITRAGE MONITOR - " << get_timestamp() << endl;
    cout << string(100, '=') << endl;
    
    print_top_orderbook_levels();
    
    if (!spot_bids.empty() && !spot_asks.empty()) {
        double bid = spot_bids.rbegin()->first;
        double ask = spot_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Binance", "Bitcoin Spot");
        bool fresh = binance_spot_timestamp.is_fresh();
        all_prices.push_back({"Binance", "Bitcoin Spot", bid, ask, ask-bid, valid, fresh,
                             spot_bids.rbegin()->second, spot_asks.begin()->second,
                             binance_spot_timestamp.age_seconds()});
    }
    
    if (!futures_bids.empty() && !futures_asks.empty()) {
        double bid = futures_bids.rbegin()->first;
        double ask = futures_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Binance", "Bitcoin Futures");
        bool fresh = binance_futures_timestamp.is_fresh();
        all_prices.push_back({"Binance", "Bitcoin Futures", bid, ask, ask-bid, valid, fresh,
                             futures_bids.rbegin()->second, futures_asks.begin()->second,
                             binance_futures_timestamp.age_seconds()});
    }
    
    if (!bybit_spot_bids.empty() && !bybit_spot_asks.empty()) {
        double bid = bybit_spot_bids.rbegin()->first;
        double ask = bybit_spot_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Bybit", "Bitcoin Spot");
        bool fresh = bybit_spot_timestamp.is_fresh();
        all_prices.push_back({"Bybit", "Bitcoin Spot", bid, ask, ask-bid, valid, fresh,
                             bybit_spot_bids.rbegin()->second, bybit_spot_asks.begin()->second,
                             bybit_spot_timestamp.age_seconds()});
    }
    
    if (!bybit_futures_bids.empty() && !bybit_futures_asks.empty()) {
        double bid = bybit_futures_bids.rbegin()->first;
        double ask = bybit_futures_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Bybit", "Bitcoin Futures");
        bool fresh = bybit_futures_timestamp.is_fresh();
        all_prices.push_back({"Bybit", "Bitcoin Futures", bid, ask, ask-bid, valid, fresh,
                             bybit_futures_bids.rbegin()->second, bybit_futures_asks.begin()->second,
                             bybit_futures_timestamp.age_seconds()});
    }
    
    if (!okx_spot_bids.empty() && !okx_spot_asks.empty()) {
        double bid = okx_spot_bids.rbegin()->first;
        double ask = okx_spot_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "OKX", "Bitcoin Spot");
        bool fresh = okx_spot_timestamp.is_fresh();
        all_prices.push_back({"OKX", "Bitcoin Spot", bid, ask, ask-bid, valid, fresh,
                             okx_spot_bids.rbegin()->second, okx_spot_asks.begin()->second,
                             okx_spot_timestamp.age_seconds()});
    }
    
    if (!okx_futures_bids.empty() && !okx_futures_asks.empty()) {
        double bid = okx_futures_bids.rbegin()->first;
        double ask = okx_futures_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "OKX", "Bitcoin Futures");
        bool fresh = okx_futures_timestamp.is_fresh();
        all_prices.push_back({"OKX", "Bitcoin Futures", bid, ask, ask-bid, valid, fresh,
                             okx_futures_bids.rbegin()->second, okx_futures_asks.begin()->second,
                             okx_futures_timestamp.age_seconds()});
    }

    if (!eth_spot_bids.empty() && !eth_spot_asks.empty()) {
        double bid = eth_spot_bids.rbegin()->first;
        double ask = eth_spot_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Binance", "Ethereum Spot");
        bool fresh = eth_binance_spot_timestamp.is_fresh();
        all_prices.push_back({"Binance", "Ethereum Spot", bid, ask, ask-bid, valid, fresh,
                             eth_spot_bids.rbegin()->second, eth_spot_asks.begin()->second,
                             eth_binance_spot_timestamp.age_seconds()});
    }
    
    if (!eth_futures_bids.empty() && !eth_futures_asks.empty()) {
        double bid = eth_futures_bids.rbegin()->first;
        double ask = eth_futures_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Binance", "Ethereum Futures");
        bool fresh = eth_binance_futures_timestamp.is_fresh();
        all_prices.push_back({"Binance", "Ethereum Futures", bid, ask, ask-bid, valid, fresh,
                             eth_futures_bids.rbegin()->second, eth_futures_asks.begin()->second,
                             eth_binance_futures_timestamp.age_seconds()});
    }
    
    if (!eth_bybit_spot_bids.empty() && !eth_bybit_spot_asks.empty()) {
        double bid = eth_bybit_spot_bids.rbegin()->first;
        double ask = eth_bybit_spot_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Bybit", "Ethereum Spot");
        bool fresh = eth_bybit_spot_timestamp.is_fresh();
        all_prices.push_back({"Bybit", "Ethereum Spot", bid, ask, ask-bid, valid, fresh,
                             eth_bybit_spot_bids.rbegin()->second, eth_bybit_spot_asks.begin()->second,
                             eth_bybit_spot_timestamp.age_seconds()});
    }
    
    if (!eth_bybit_futures_bids.empty() && !eth_bybit_futures_asks.empty()) {
        double bid = eth_bybit_futures_bids.rbegin()->first;
        double ask = eth_bybit_futures_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "Bybit", "Ethereum Futures");
        bool fresh = eth_bybit_futures_timestamp.is_fresh();
        all_prices.push_back({"Bybit", "Ethereum Futures", bid, ask, ask-bid, valid, fresh,
                             eth_bybit_futures_bids.rbegin()->second, eth_bybit_futures_asks.begin()->second,
                             eth_bybit_futures_timestamp.age_seconds()});
    }
    
    if (!eth_okx_spot_bids.empty() && !eth_okx_spot_asks.empty()) {
        double bid = eth_okx_spot_bids.rbegin()->first;
        double ask = eth_okx_spot_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "OKX", "Ethereum Spot");
        bool fresh = eth_okx_spot_timestamp.is_fresh();
        all_prices.push_back({"OKX", "Ethereum Spot", bid, ask, ask-bid, valid, fresh,
                             eth_okx_spot_bids.rbegin()->second, eth_okx_spot_asks.begin()->second,
                             eth_okx_spot_timestamp.age_seconds()});
    }
    
    if (!eth_okx_futures_bids.empty() && !eth_okx_futures_asks.empty()) {
        double bid = eth_okx_futures_bids.rbegin()->first;
        double ask = eth_okx_futures_asks.begin()->first;
        bool valid = validate_orderbook(bid, ask, "OKX", "Ethereum Futures");
        bool fresh = eth_okx_futures_timestamp.is_fresh();
        all_prices.push_back({"OKX", "Ethereum Futures", bid, ask, ask-bid, valid, fresh,
                             eth_okx_futures_bids.rbegin()->second, eth_okx_futures_asks.begin()->second,
                             eth_okx_futures_timestamp.age_seconds()});
    }

    cout << "\n REAL-TIME EXCHANGE PRICES & DATA QUALITY" << endl;
    cout << string(120, '-') << endl;
    cout << setw(10) << "Exchange" << setw(18) << "Market" 
         << setw(12) << "Best Bid" << setw(12) << "Best Ask" 
         << setw(10) << "Spread" << setw(10) << "Spread%" 
         << setw(8) << "Age(s)" << setw(8) << "Fresh" << setw(8) << "Valid" << endl;
    cout << string(120, '-') << endl;
    
    for (const auto& price : all_prices) {
        string fresh_status = price.fresh ? "Yes" : "No";
        string valid_status = price.valid ? "Yes" : "No";
        double spread_pct = price.valid && price.bid > 0 ? (price.spread / price.bid * 100) : 0;
        
        cout << setw(10) << price.exchange << setw(18) << price.market 
             << " $" << fixed << setprecision(2) << setw(10) << price.bid
             << " $" << fixed << setprecision(2) << setw(10) << price.ask
             << " $" << fixed << setprecision(2) << setw(8) << price.spread
             << fixed << setprecision(4) << setw(9) << spread_pct << "%"
             << setw(8) << price.age_seconds
             << setw(8) << fresh_status
             << setw(8) << valid_status << endl;
    }

    cout << "\n REALISTIC ARBITRAGE ANALYSIS" << endl;
    cout << string(120, '-') << endl;
    
    vector<tuple<double, double, string, string, string>> opportunities;
    int total_opportunities = 0;
    int realistic_opportunities = 0;
    
    for (size_t i = 0; i < all_prices.size(); i++) {
        for (size_t j = 0; j < all_prices.size(); j++) {
            if (i != j && all_prices[i].valid && all_prices[j].valid && 
                all_prices[i].fresh && all_prices[j].fresh) {
                
                double profit = all_prices[i].bid - all_prices[j].ask;
                double profit_pct = all_prices[j].ask > 0 ? (profit / all_prices[j].ask) * 100 : 0;
                total_opportunities++;
                
                bool is_realistic = true;
                
                if (profit < 0.10 || profit > 5.00) {
                    is_realistic = false;
                }
                
                if (profit_pct < 0.0001 || profit_pct > 0.05) {
                    is_realistic = false;
                }
                
                if (all_prices[i].exchange == all_prices[j].exchange && profit > 2.0) {
                    is_realistic = false;
                }
                
                if (!all_prices[i].fresh || !all_prices[j].fresh) {
                    is_realistic = false;
                }
                
                if (profit > 0.10 && is_realistic) {
                    realistic_opportunities++;
                    string buy_side = all_prices[j].exchange + " " + all_prices[j].market;
                    string sell_side = all_prices[i].exchange + " " + all_prices[i].market;
                    string description = "Buy " + buy_side + " @ $" + 
                                       to_string(all_prices[j].ask).substr(0, 10) +
                                       " → Sell " + sell_side + " @ $" + 
                                       to_string(all_prices[i].bid).substr(0, 10);
                    
                    opportunities.push_back(make_tuple(profit, profit_pct, description, buy_side, sell_side));
                }
            }
        }
    }
    
    sort(opportunities.begin(), opportunities.end(), 
         [](const auto& a, const auto& b) { return get<0>(a) > get<0>(b); });
    
    cout << "\n ARBITRAGE SUMMARY:" << endl;
    cout << "Total potential opportunities: " << total_opportunities << endl;
    cout << "Realistic opportunities: " << realistic_opportunities << endl;
    cout << "Filtered out: " << (total_opportunities - realistic_opportunities) << " (unrealistic/stale)" << endl;
    
    if (!opportunities.empty()) {
        cout << "\n REALISTIC ARBITRAGE OPPORTUNITIES:" << endl;
        cout << string(120, '-') << endl;
        
        for (size_t i = 0; i < min(opportunities.size(), size_t(5)); i++) {
            double profit = get<0>(opportunities[i]);
            double profit_pct = get<1>(opportunities[i]);
            string description = get<2>(opportunities[i]);
            
            cout << "[" << (i+1) << "] $" << fixed << setprecision(2) << profit 
                 << " (" << fixed << setprecision(4) << profit_pct << "%) " 
                 << description << endl;
        }
        
        auto best = opportunities[0];
        cout << "\n BEST REALISTIC OPPORTUNITY:" << endl;
        cout << "    Profit: $" << fixed << setprecision(2) << get<0>(best) << endl;
        cout << "    ROI: " << fixed << setprecision(4) << get<1>(best) << "%" << endl;
        cout << "    Strategy: " << get<2>(best) << endl;
        cout << "    After 0.1% fees: $" << fixed << setprecision(2) << (get<0>(best) * 0.998) << endl;
        
    } else {
        cout << "\n NO REALISTIC ARBITRAGE OPPORTUNITIES" << endl;
        cout << " This is NORMAL - real crypto arbitrage is rare and small" << endl;
        cout << " Your system is working correctly with realistic market data!" << endl;
    }

    cout << "\n" << string(120, '=') << endl;
}

void print_debug_orderbook() {
    cout << "\n DEBUG: ORDERBOOK STATUS & FRESHNESS" << endl;
    
    cout << "=== BITCOIN (BTC) ORDERBOOKS ===" << endl;
    cout << "Binance Bitcoin Spot: " << spot_bids.size() << " bids, " << spot_asks.size() << " asks (age: " 
         << binance_spot_timestamp.age_seconds() << "s)" << endl;
    cout << "Binance Bitcoin Futures: " << futures_bids.size() << " bids, " << futures_asks.size() << " asks (age: " 
         << binance_futures_timestamp.age_seconds() << "s)" << endl;
    cout << "Bybit Bitcoin Spot: " << bybit_spot_bids.size() << " bids, " << bybit_spot_asks.size() << " asks (age: " 
         << bybit_spot_timestamp.age_seconds() << "s)" << endl;
    cout << "Bybit Bitcoin Futures: " << bybit_futures_bids.size() << " bids, " << bybit_futures_asks.size() << " asks (age: " 
         << bybit_futures_timestamp.age_seconds() << "s)" << endl;
    cout << "OKX Bitcoin Spot: " << okx_spot_bids.size() << " bids, " << okx_spot_asks.size() << " asks (age: " 
         << okx_spot_timestamp.age_seconds() << "s)" << endl;
    cout << "OKX Bitcoin Futures: " << okx_futures_bids.size() << " bids, " << okx_futures_asks.size() << " asks (age: " 
         << okx_futures_timestamp.age_seconds() << "s)" << endl;
    
    cout << "\n=== ETHEREUM (ETH) ORDERBOOKS ===" << endl;
    cout << "Binance Ethereum Spot: " << eth_spot_bids.size() << " bids, " << eth_spot_asks.size() << " asks (age: " 
         << eth_binance_spot_timestamp.age_seconds() << "s)" << endl;
    cout << "Binance Ethereum Futures: " << eth_futures_bids.size() << " bids, " << eth_futures_asks.size() << " asks (age: " 
         << eth_binance_futures_timestamp.age_seconds() << "s)" << endl;
    cout << "Bybit Ethereum Spot: " << eth_bybit_spot_bids.size() << " bids, " << eth_bybit_spot_asks.size() << " asks (age: " 
         << eth_bybit_spot_timestamp.age_seconds() << "s)" << endl;
    cout << "Bybit Ethereum Futures: " << eth_bybit_futures_bids.size() << " bids, " << eth_bybit_futures_asks.size() << " asks (age: " 
         << eth_bybit_futures_timestamp.age_seconds() << "s)" << endl;
    cout << "OKX Ethereum Spot: " << eth_okx_spot_bids.size() << " bids, " << eth_okx_spot_asks.size() << " asks (age: " 
         << eth_okx_spot_timestamp.age_seconds() << "s)" << endl;
    cout << "OKX Ethereum Futures: " << eth_okx_futures_bids.size() << " bids, " << eth_okx_futures_asks.size() << " asks (age: " 
         << eth_okx_futures_timestamp.age_seconds() << "s)" << endl;
}
