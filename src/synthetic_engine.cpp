#include "synthetic_engine.hpp"
#include "book_storage.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <vector>
#include <algorithm>

using namespace std;

SyntheticEngine* global_synthetic_engine = nullptr;

SyntheticEngine::SyntheticEngine(const ConfigThresholds& cfg) : config(cfg) {
    global_synthetic_engine = this;
}

SyntheticEngine::~SyntheticEngine() {
    stop();
    global_synthetic_engine = nullptr;
}

void SyntheticEngine::start() {
    running = true;
    calculation_thread = thread(&SyntheticEngine::calculation_loop, this);
    cout << "[SYNTHETIC ENGINE] Started synthetic pricing calculations" << endl;
}

void SyntheticEngine::stop() {
    running = false;
    if (calculation_thread.joinable()) {
        calculation_thread.join();
    }
    cout << "[SYNTHETIC ENGINE]  Stopped synthetic pricing calculations" << endl;
}

double SyntheticEngine::calculate_synthetic_spot_from_perp(double perp_price, double funding_rate) {
    double daily_funding_adjustment = perp_price * funding_rate;
    return perp_price - daily_funding_adjustment;
}

double SyntheticEngine::calculate_synthetic_futures_price(double spot_price, double time_to_expiry, double rate) {
    return spot_price * exp(rate * time_to_expiry);
}

double SyntheticEngine::calculate_mispricing_percent(double real_price, double synthetic_price) {
    if (synthetic_price == 0.0) return 0.0;
    return ((real_price - synthetic_price) / synthetic_price) * 100.0;
}

double SyntheticEngine::get_current_funding_rate(const string& exchange, const string& symbol) {
    lock_guard<mutex> lock(funding_mutex);
    string key = exchange + "_" + symbol;
    auto it = funding_rates.find(key);
    if (it != funding_rates.end()) {
        return it->second;
    }
    return config.default_funding_rate;
}

double SyntheticEngine::estimate_time_to_expiry() {
    return 0.25; // 3 months
}

string SyntheticEngine::generate_key(const string& exchange, const string& market) {
    return exchange + "_" + market;
}



void SyntheticEngine::calculation_loop() {
    while (running) {
        try {
            {
                lock_guard<mutex> book_lock(book_mutex);
                lock_guard<mutex> synthetic_lock(synthetic_mutex);
                
                // **BITCOIN CALCULATIONS**
                
                // **BINANCE BITCOIN**
                if (!spot_bids.empty() && !spot_asks.empty() && 
                    !futures_bids.empty() && !futures_asks.empty()) {
                    
                    double spot_mid = (spot_bids.rbegin()->first + spot_asks.begin()->first) / 2.0;
                    double futures_mid = (futures_bids.rbegin()->first + futures_asks.begin()->first) / 2.0;
                    
                    double synthetic_future = calculate_synthetic_futures_price(
                        spot_mid, estimate_time_to_expiry(), config.risk_free_rate);
                    
                    double mispricing = calculate_mispricing_percent(futures_mid, synthetic_future);
                    
                    SyntheticPrice binance_calc;
                    binance_calc.real_price = futures_mid;
                    binance_calc.synthetic_price = synthetic_future;
                    binance_calc.mispricing_percent = mispricing;
                    binance_calc.funding_rate = config.risk_free_rate;
                    binance_calc.exchange = "Binance";
                    binance_calc.instrument_type = "Bitcoin futures_vs_spot";  
                    binance_calc.timestamp = chrono::steady_clock::now();
                    binance_calc.is_valid = abs(mispricing) >= config.min_mispricing_percent;
                    
                    synthetic_prices["Binance_Bitcoin_futures_vs_spot"] = binance_calc;
                }
                
                // **OKX BITCOIN**
                if (!okx_spot_bids.empty() && !okx_spot_asks.empty() && 
                    !okx_futures_bids.empty() && !okx_futures_asks.empty()) {
                    
                    double spot_mid = (okx_spot_bids.rbegin()->first + okx_spot_asks.begin()->first) / 2.0;
                    double futures_mid = (okx_futures_bids.rbegin()->first + okx_futures_asks.begin()->first) / 2.0;
                    
                    double synthetic_future = calculate_synthetic_futures_price(
                        spot_mid, estimate_time_to_expiry(), config.risk_free_rate);
                    
                    double mispricing = calculate_mispricing_percent(futures_mid, synthetic_future);
                    
                    SyntheticPrice okx_calc;
                    okx_calc.real_price = futures_mid;
                    okx_calc.synthetic_price = synthetic_future;
                    okx_calc.mispricing_percent = mispricing;
                    okx_calc.funding_rate = config.risk_free_rate;
                    okx_calc.exchange = "OKX";
                    okx_calc.instrument_type = "Bitcoin futures_vs_spot";  
                    okx_calc.timestamp = chrono::steady_clock::now();
                    okx_calc.is_valid = abs(mispricing) >= config.min_mispricing_percent;
                    
                    synthetic_prices["OKX_Bitcoin_futures_vs_spot"] = okx_calc;
                }
                
                // **BYBIT BITCOIN** (keeping original perpetual logic)
                if (!bybit_spot_bids.empty() && !bybit_spot_asks.empty() && 
                    !bybit_futures_bids.empty() && !bybit_futures_asks.empty()) {
                    
                    double spot_mid = (bybit_spot_bids.rbegin()->first + bybit_spot_asks.begin()->first) / 2.0;
                    double futures_mid = (bybit_futures_bids.rbegin()->first + bybit_futures_asks.begin()->first) / 2.0;
                    
                    double funding_rate = get_current_funding_rate("Bybit", "BTCUSDT");
                    double synthetic_spot = calculate_synthetic_spot_from_perp(futures_mid, funding_rate);
                    
                    double mispricing = calculate_mispricing_percent(spot_mid, synthetic_spot);
                    
                    SyntheticPrice bybit_calc;
                    bybit_calc.real_price = spot_mid;
                    bybit_calc.synthetic_price = synthetic_spot;
                    bybit_calc.mispricing_percent = mispricing;
                    bybit_calc.funding_rate = funding_rate;
                    bybit_calc.exchange = "Bybit";
                    bybit_calc.instrument_type = "Bitcoin spot_vs_perpetual"; 
                    bybit_calc.timestamp = chrono::steady_clock::now();
                    bybit_calc.is_valid = abs(mispricing) >= config.min_mispricing_percent;
                    
                    synthetic_prices["Bybit_Bitcoin_spot_vs_perpetual"] = bybit_calc;
                }
                
                
                // **BINANCE ETHEREUM**
                if (!eth_spot_bids.empty() && !eth_spot_asks.empty() && 
                    !eth_futures_bids.empty() && !eth_futures_asks.empty()) {
                    
                    double spot_mid = (eth_spot_bids.rbegin()->first + eth_spot_asks.begin()->first) / 2.0;
                    double futures_mid = (eth_futures_bids.rbegin()->first + eth_futures_asks.begin()->first) / 2.0;
                    
                    double synthetic_future = calculate_synthetic_futures_price(
                        spot_mid, estimate_time_to_expiry(), config.risk_free_rate);
                    
                    double mispricing = calculate_mispricing_percent(futures_mid, synthetic_future);
                    
                    SyntheticPrice binance_eth_calc;
                    binance_eth_calc.real_price = futures_mid;
                    binance_eth_calc.synthetic_price = synthetic_future;
                    binance_eth_calc.mispricing_percent = mispricing;
                    binance_eth_calc.funding_rate = config.risk_free_rate;
                    binance_eth_calc.exchange = "Binance";
                    binance_eth_calc.instrument_type = "Ethereum futures_vs_spot";  
                    binance_eth_calc.timestamp = chrono::steady_clock::now();
                    binance_eth_calc.is_valid = abs(mispricing) >= config.min_mispricing_percent;
                    
                    synthetic_prices["Binance_Ethereum_futures_vs_spot"] = binance_eth_calc;
                }
                
                // **OKX ETHEREUM**
                if (!eth_okx_spot_bids.empty() && !eth_okx_spot_asks.empty() && 
                    !eth_okx_futures_bids.empty() && !eth_okx_futures_asks.empty()) {
                    
                    double spot_mid = (eth_okx_spot_bids.rbegin()->first + eth_okx_spot_asks.begin()->first) / 2.0;
                    double futures_mid = (eth_okx_futures_bids.rbegin()->first + eth_okx_futures_asks.begin()->first) / 2.0;
                    
                    double synthetic_future = calculate_synthetic_futures_price(
                        spot_mid, estimate_time_to_expiry(), config.risk_free_rate);
                    
                    double mispricing = calculate_mispricing_percent(futures_mid, synthetic_future);
                    
                    SyntheticPrice okx_eth_calc;
                    okx_eth_calc.real_price = futures_mid;
                    okx_eth_calc.synthetic_price = synthetic_future;
                    okx_eth_calc.mispricing_percent = mispricing;
                    okx_eth_calc.funding_rate = config.risk_free_rate;
                    okx_eth_calc.exchange = "OKX";
                    okx_eth_calc.instrument_type = "Ethereum futures_vs_spot"; 
                    okx_eth_calc.timestamp = chrono::steady_clock::now();
                    okx_eth_calc.is_valid = abs(mispricing) >= config.min_mispricing_percent;
                    
                    synthetic_prices["OKX_Ethereum_futures_vs_spot"] = okx_eth_calc;
                }
                
                if (!eth_bybit_spot_bids.empty() && !eth_bybit_spot_asks.empty() && 
                    !eth_bybit_futures_bids.empty() && !eth_bybit_futures_asks.empty()) {
                    
                    double spot_mid = (eth_bybit_spot_bids.rbegin()->first + eth_bybit_spot_asks.begin()->first) / 2.0;
                    double futures_mid = (eth_bybit_futures_bids.rbegin()->first + eth_bybit_futures_asks.begin()->first) / 2.0;
                    
                    double funding_rate = get_current_funding_rate("Bybit", "ETHUSDT");
                    double synthetic_spot = calculate_synthetic_spot_from_perp(futures_mid, funding_rate);
                    
                    double mispricing = calculate_mispricing_percent(spot_mid, synthetic_spot);
                    
                    SyntheticPrice bybit_eth_calc;
                    bybit_eth_calc.real_price = spot_mid;
                    bybit_eth_calc.synthetic_price = synthetic_spot;
                    bybit_eth_calc.mispricing_percent = mispricing;
                    bybit_eth_calc.funding_rate = funding_rate;
                    bybit_eth_calc.exchange = "Bybit";
                    bybit_eth_calc.instrument_type = "Ethereum spot_vs_perpetual"; 
                    bybit_eth_calc.timestamp = chrono::steady_clock::now();
                    bybit_eth_calc.is_valid = abs(mispricing) >= config.min_mispricing_percent;
                    
                    synthetic_prices["Bybit_Ethereum_spot_vs_perpetual"] = bybit_eth_calc;
                }
            }
            
            if (config.enable_logging) {
                log_synthetic_calculations();
            }
            
        } catch (const exception& e) {
            cout << "[SYNTHETIC ENGINE ERROR] " << e.what() << endl;
        }
        
        this_thread::sleep_for(chrono::milliseconds(config.calculation_interval_ms));
    }
}

vector<SyntheticPrice> SyntheticEngine::get_mispricing_opportunities() {
    lock_guard<mutex> lock(synthetic_mutex);
    vector<SyntheticPrice> opportunities;
    
    for (const auto& pair : synthetic_prices) {
        if (pair.second.is_valid && 
            abs(pair.second.mispricing_percent) >= config.min_mispricing_percent &&
            abs(pair.second.mispricing_percent) <= config.max_mispricing_percent) {
            opportunities.push_back(pair.second);
        }
    }
    
    sort(opportunities.begin(), opportunities.end(), 
         [](const SyntheticPrice& a, const SyntheticPrice& b) {
             return abs(a.mispricing_percent) > abs(b.mispricing_percent);
         });
    
    return opportunities;
}

void SyntheticEngine::print_mispricing_opportunities() {
    auto opportunities = get_mispricing_opportunities();
    
    cout << "\n SYNTHETIC PRICING & MISPRICING ANALYSIS" << endl;
    cout << string(100, '=') << endl;
    
    if (opportunities.empty()) {
        cout << " NO SIGNIFICANT MISPRICING DETECTED" << endl;
        cout << " All instruments trading close to synthetic fair value" << endl;
        cout << " Threshold: ±" << fixed << setprecision(3) << config.min_mispricing_percent << "%" << endl;
    } else {
        cout << " MISPRICING OPPORTUNITIES DETECTED:" << endl;
        cout << string(120, '-') << endl;
        cout << setw(10) << "Exchange" << setw(25) << "Strategy"  
             << setw(12) << "Real Price" << setw(15) << "Fair Price"
             << setw(12) << "Mispricing%" << setw(50) << "EXACT TRADING ACTION" << endl;
        cout << string(120, '-') << endl;
        
        for (const auto& opp : opportunities) {
            string exact_action;
            
            string asset_type = "Bitcoin";  
            if (opp.instrument_type.find("Ethereum") != string::npos) {
                asset_type = "Ethereum";
            }
            
            if (opp.instrument_type.find("futures_vs_spot") != string::npos) {
                if (opp.mispricing_percent > 0) {
                    exact_action = "SELL " + opp.exchange + " " + asset_type + " futures + BUY " + opp.exchange + " " + asset_type + " spot";
                } else {
                    exact_action = "BUY " + opp.exchange + " " + asset_type + " futures + SELL " + opp.exchange + " " + asset_type + " spot";
                }
            } else if (opp.instrument_type.find("spot_vs_perpetual") != string::npos) {
                if (opp.mispricing_percent > 0) {
                    exact_action = "SELL " + opp.exchange + " " + asset_type + " spot + BUY " + opp.exchange + " " + asset_type + " perpetual";
                } else {
                    exact_action = "BUY " + opp.exchange + " " + asset_type + " spot + SELL " + opp.exchange + " " + asset_type + " perpetual";
                }
            }
            
            cout << setw(10) << opp.exchange 
                 << setw(25) << opp.instrument_type 
                 << " $" << fixed << setprecision(2) << setw(10) << opp.real_price
                 << " $" << fixed << setprecision(2) << setw(13) << opp.synthetic_price
                 << fixed << setprecision(3) << setw(11) << opp.mispricing_percent << "%"
                 << setw(50) << exact_action << endl;
        }
        
        cout << "\n DETAILED TRADING INSTRUCTIONS:" << endl;
        for (size_t i = 0; i < opportunities.size() && i < 3; ++i) {
            const auto& opp = opportunities[i];
            
            cout << "\n[OPPORTUNITY " << (i+1) << "] " << opp.exchange << " " << opp.instrument_type << endl;
            cout << "   Real Price: $" << fixed << setprecision(2) << opp.real_price << endl;
            cout << "   Fair Price: $" << fixed << setprecision(2) << opp.synthetic_price << endl;
            cout << "   Mispricing: " << fixed << setprecision(3) << opp.mispricing_percent << "%" << endl;
            
            string asset_type = "Bitcoin";
            if (opp.instrument_type.find("Ethereum") != string::npos) {
                asset_type = "Ethereum";
            }
            
            if (opp.instrument_type.find("futures_vs_spot") != string::npos) {
                if (opp.mispricing_percent < 0) {
                    cout << "   Action: " << asset_type << " futures are UNDERPRICED by $" << fixed << setprecision(2) 
                         << abs(opp.synthetic_price - opp.real_price) << endl;
                    cout << "   Execute: BUY " << opp.exchange << " " << asset_type << " futures @ $" << opp.real_price 
                         << " + SELL " << opp.exchange << " " << asset_type << " spot" << endl;
                    cout << "   Expected: Profit when futures price rises to fair value" << endl;
                } else {
                    cout << "   Action: " << asset_type << " futures are OVERPRICED by $" << fixed << setprecision(2) 
                         << abs(opp.real_price - opp.synthetic_price) << endl;
                    cout << "   Execute: SELL " << opp.exchange << " " << asset_type << " futures @ $" << opp.real_price 
                         << " + BUY " << opp.exchange << " " << asset_type << " spot" << endl;
                    cout << "   Expected: Profit when futures price drops to fair value" << endl;
                }
            } else if (opp.instrument_type.find("spot_vs_perpetual") != string::npos) {
                if (opp.mispricing_percent > 0) {
                    cout << "   Action: " << asset_type << " spot is OVERPRICED by $" << fixed << setprecision(2) 
                         << abs(opp.real_price - opp.synthetic_price) << endl;
                    cout << "   Execute: SELL " << opp.exchange << " " << asset_type << " spot @ $" << opp.real_price 
                         << " + BUY " << opp.exchange << " " << asset_type << " perpetual" << endl;
                    cout << "   Expected: Profit when prices converge" << endl;
                } else {
                    cout << "   Action: " << asset_type << " spot is UNDERPRICED by $" << fixed << setprecision(2) 
                         << abs(opp.synthetic_price - opp.real_price) << endl;
                    cout << "   Execute: BUY " << opp.exchange << " " << asset_type << " spot @ $" << opp.real_price 
                         << " + SELL " << opp.exchange << " " << asset_type << " perpetual" << endl;
                    cout << "   Expected: Profit when prices converge" << endl;
                }
            }
        }
    }
    
    cout << string(100, '=') << endl;
}

void SyntheticEngine::log_synthetic_calculations() {
    static int log_counter = 0;
    log_counter++;
    
    if (log_counter % 10 == 0) {
        lock_guard<mutex> lock(synthetic_mutex);
        
        cout << "\n [SYNTHETIC ENGINE] Calculation Summary #" << log_counter << endl;
        
        if (synthetic_prices.empty()) {
            cout << "  No synthetic calculations available yet..." << endl;
            return;
        }
        
        for (const auto& pair : synthetic_prices) {
            const auto& calc = pair.second;
            string status = calc.is_valid ? " No" : " Yes";
            
            cout << "  " << calc.exchange << " " << calc.instrument_type 
                 << ": " << fixed << setprecision(3) << calc.mispricing_percent 
                 << "% mispricing" << status << endl;
        }
    }
}

void SyntheticEngine::update_funding_rate(const string& exchange, const string& symbol, double rate) {
    lock_guard<mutex> lock(funding_mutex);
    string key = exchange + "_" + symbol;
    funding_rates[key] = rate;
    cout << "[SYNTHETIC ENGINE] Updated funding rate: " << key << " = " 
         << fixed << setprecision(4) << (rate * 100) << "%" << endl;
}

void SyntheticEngine::set_config(const ConfigThresholds& cfg) {
    config = cfg;
    cout << "[SYNTHETIC ENGINE] Configuration updated" << endl;
}

SyntheticPrice SyntheticEngine::get_synthetic_price(const string& exchange, const string& market) {
    lock_guard<mutex> lock(synthetic_mutex);
    string key = generate_key(exchange, market);
    auto it = synthetic_prices.find(key);
    if (it != synthetic_prices.end()) {
        return it->second;
    }
    return SyntheticPrice{};
}
