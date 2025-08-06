#include "binance_ws.hpp"
#include "binance_futures_ws.hpp"
#include "bybit_ws.hpp"
#include "okx_ws.hpp"
#include "book_storage.hpp"
#include "synthetic_engine.hpp"
#include "risk_manager.hpp"
#include "performance_monitor.hpp"
#include "connection_pool.hpp"
#include "multi_leg_arbitrage.hpp"
#include "simd_optimizer.hpp"
#include "eth_binance_ws.hpp"
#include "eth_bybit_ws.hpp"
#include "eth_okx_ws.hpp"
#include "real_volatility_arbitrage.hpp"
#include "real_cross_asset_arbitrage.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <iomanip>

using namespace std;

volatile bool running = true;

void signal_handler(int /* sig */) {
    running = false;
}

void update_real_analyzers(RealVolatilityArbitrage& real_vol_analyzer, 
                          RealCrossAssetArbitrage& real_cross_analyzer,
                          MultiLegArbitrageEngine& multi_leg_engine) {
    lock_guard<mutex> lock(book_mutex);
    
    if (!spot_bids.empty() && !spot_asks.empty() &&
        !futures_bids.empty() && !futures_asks.empty()) {
        
        double binance_btc_spot = (spot_bids.rbegin()->first + spot_asks.begin()->first) / 2.0;
        double binance_btc_futures = (futures_bids.rbegin()->first + futures_asks.begin()->first) / 2.0;
        
        real_vol_analyzer.update_market_data("Binance", "Bitcoin", binance_btc_spot, binance_btc_futures);
        real_cross_analyzer.update_asset_price("Binance", "Bitcoin", binance_btc_spot);
        multi_leg_engine.update_market_data("Binance", "Bitcoin", binance_btc_spot, binance_btc_futures); 
    }
    
    if (!eth_spot_bids.empty() && !eth_spot_asks.empty() &&
        !eth_futures_bids.empty() && !eth_futures_asks.empty()) {
        
        double binance_eth_spot = (eth_spot_bids.rbegin()->first + eth_spot_asks.begin()->first) / 2.0;
        double binance_eth_futures = (eth_futures_bids.rbegin()->first + eth_futures_asks.begin()->first) / 2.0;
        
        real_vol_analyzer.update_market_data("Binance", "Ethereum", binance_eth_spot, binance_eth_futures);
        real_cross_analyzer.update_asset_price("Binance", "Ethereum", binance_eth_spot);
        multi_leg_engine.update_market_data("Binance", "Ethereum", binance_eth_spot, binance_eth_futures); 
    }
    
  
    if (!bybit_spot_bids.empty() && !bybit_spot_asks.empty() &&
        !bybit_futures_bids.empty() && !bybit_futures_asks.empty()) {
        
        double bybit_btc_spot = (bybit_spot_bids.rbegin()->first + bybit_spot_asks.begin()->first) / 2.0;
        double bybit_btc_futures = (bybit_futures_bids.rbegin()->first + bybit_futures_asks.begin()->first) / 2.0;
        
        real_vol_analyzer.update_market_data("Bybit", "Bitcoin", bybit_btc_spot, bybit_btc_futures);
        real_cross_analyzer.update_asset_price("Bybit", "Bitcoin", bybit_btc_spot);
        multi_leg_engine.update_market_data("Bybit", "Bitcoin", bybit_btc_spot, bybit_btc_futures);
    }
    
    if (!eth_bybit_spot_bids.empty() && !eth_bybit_spot_asks.empty() &&
        !eth_bybit_futures_bids.empty() && !eth_bybit_futures_asks.empty()) {
        
        double bybit_eth_spot = (eth_bybit_spot_bids.rbegin()->first + eth_bybit_spot_asks.begin()->first) / 2.0;
        double bybit_eth_futures = (eth_bybit_futures_bids.rbegin()->first + eth_bybit_futures_asks.begin()->first) / 2.0;
        
        real_vol_analyzer.update_market_data("Bybit", "Ethereum", bybit_eth_spot, bybit_eth_futures);
        real_cross_analyzer.update_asset_price("Bybit", "Ethereum", bybit_eth_spot);
        multi_leg_engine.update_market_data("Bybit", "Ethereum", bybit_eth_spot, bybit_eth_futures); 
    }
    
    
    if (!okx_spot_bids.empty() && !okx_spot_asks.empty() &&
        !okx_futures_bids.empty() && !okx_futures_asks.empty()) {
        
        double okx_btc_spot = (okx_spot_bids.rbegin()->first + okx_spot_asks.begin()->first) / 2.0;
        double okx_btc_futures = (okx_futures_bids.rbegin()->first + okx_futures_asks.begin()->first) / 2.0;
        
        real_vol_analyzer.update_market_data("OKX", "Bitcoin", okx_btc_spot, okx_btc_futures);
        real_cross_analyzer.update_asset_price("OKX", "Bitcoin", okx_btc_spot);
        multi_leg_engine.update_market_data("OKX", "Bitcoin", okx_btc_spot, okx_btc_futures); 
    }
    
    if (!eth_okx_spot_bids.empty() && !eth_okx_spot_asks.empty() &&
        !eth_okx_futures_bids.empty() && !eth_okx_futures_asks.empty()) {
        
        double okx_eth_spot = (eth_okx_spot_bids.rbegin()->first + eth_okx_spot_asks.begin()->first) / 2.0;
        double okx_eth_futures = (eth_okx_futures_bids.rbegin()->first + eth_okx_futures_asks.begin()->first) / 2.0;
        
        real_vol_analyzer.update_market_data("OKX", "Ethereum", okx_eth_spot, okx_eth_futures);
        real_cross_analyzer.update_asset_price("OKX", "Ethereum", okx_eth_spot);
        multi_leg_engine.update_market_data("OKX", "Ethereum", okx_eth_spot, okx_eth_futures); 
    }
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    cout << "Deviant a high performance Multi-Exchange Arbitrage System is Starting..." << endl;
    
    PerformanceMonitor::start_monitoring();
    
    MultiLegArbitrageEngine* multi_leg_engine = nullptr;
    try {
        multi_leg_engine = new MultiLegArbitrageEngine();
        cout << " Multi-leg arbitrage engine initialized successfully." << endl;
    } catch (const exception& e) {
        cout << "[ERROR] Failed to initialize multi-leg engine: " << e.what() << endl;
    }
    
    try {
        SIMDOptimizer::print_simd_capabilities();
        cout << "SIMD optimizer initialized successfully." << endl;
    } catch (const exception& e) {
        cout << "[ERROR] Failed to initialize SIMD optimizer: " << e.what() << endl;
    }
    
    ConnectionPool connection_pool(6);
    
    RiskConfig risk_config;
    risk_config.initial_capital = 10000.0;
    risk_config.max_risk_per_trade = 0.01;
    risk_config.enable_position_sizing = true;
    RiskManager risk_manager(risk_config);
    
    ConfigThresholds config;
    config.min_mispricing_percent = 0.05;
    config.risk_free_rate = 0.001;
    config.calculation_interval_ms = 100;
    config.enable_logging = false;
    SyntheticEngine synthetic_engine(config);
    
    RealVolatilityArbitrage real_vol_analyzer;
    RealCrossAssetArbitrage real_cross_analyzer;
    cout << "REAL advanced arbitrage engines initialized - NO SIMULATION DATA." << endl;
    
    connection_pool.add_connection("Binance_Spot", "wss://stream.binance.com:9443/ws/btcusdt@depth",
        [](string data) {
            PERF_TIMER("parse_binance_spot");
            (void)data;
        });
    
    connection_pool.add_connection("Binance_Futures", "wss://fstream.binance.com/ws/btcusdt@depth",
        [](string data) {
            PERF_TIMER("parse_binance_futures");
            (void)data;
        });
    
    connection_pool.start_all_connections();
    
    thread binance_spot_thread([]() { try { start_binance_ws(); } catch (const exception& e) { cout << "[ERROR] Binance spot thread error: " << e.what() << endl; }});
    thread binance_futures_thread([]() { try { start_binance_futures_ws(); } catch (const exception& e) { cout << "[ERROR] Binance futures thread error: " << e.what() << endl; }});
    thread bybit_spot_thread([]() { try { start_bybit_spot_ws(); } catch (const exception& e) { cout << "[ERROR] Bybit spot thread error: " << e.what() << endl; }});
    thread bybit_futures_thread([]() { try { start_bybit_futures_ws(); } catch (const exception& e) { cout << "[ERROR] Bybit futures thread error: " << e.what() << endl; }});
    thread okx_spot_thread([]() { try { start_okx_spot_ws(); } catch (const exception& e) { cout << "[ERROR] OKX spot thread error: " << e.what() << endl; }});
    thread okx_futures_thread([]() { try { start_okx_futures_ws(); } catch (const exception& e) { cout << "[ERROR] OKX futures thread error: " << e.what() << endl; }});
    thread eth_binance_spot_thread([]() { try { start_eth_binance_spot_ws(); } catch (const exception& e) { cout << "[ERROR] ETH Binance spot thread error: " << e.what() << endl; }});
    thread eth_binance_futures_thread([]() { try { start_eth_binance_futures_ws(); } catch (const exception& e) { cout << "[ERROR] ETH Binance futures thread error: " << e.what() << endl; }});
    thread eth_bybit_spot_thread([]() { try { start_eth_bybit_spot_ws(); } catch (const exception& e) { cout << "[ERROR] ETH Bybit spot thread error: " << e.what() << endl; }});
    thread eth_bybit_futures_thread([]() { try { start_eth_bybit_futures_ws(); } catch (const exception& e) { cout << "[ERROR] ETH Bybit futures thread error: " << e.what() << endl; }});
    thread eth_okx_spot_thread([]() { try { start_eth_okx_spot_ws(); } catch (const exception& e) { cout << "[ERROR] ETH OKX spot thread error: " << e.what() << endl; }});
    thread eth_okx_futures_thread([]() { try { start_eth_okx_futures_ws(); } catch (const exception& e) { cout << "[ERROR] ETH OKX futures thread error: " << e.what() << endl; }});
    
    this_thread::sleep_for(chrono::seconds(5));
    
    synthetic_engine.start();
    cout << "Starting HIGH-PERFORMANCE analysis with REAL BONUS FEATURES..." << endl;
    
    int cycle_count = 0;
    auto last_performance_report = chrono::high_resolution_clock::now();
    
    while (running) {
        try {
            PERF_TIMER("main_analysis_cycle");
            cycle_count++;
            
            if (cycle_count % 10 == 0) {
                {
                    PERF_TIMER("arbitrage_analysis");
                    lock_guard<mutex> lock(book_mutex);
                    print_all_books();
                }
                
                if (multi_leg_engine != nullptr) {
                    update_real_analyzers(real_vol_analyzer, real_cross_analyzer, *multi_leg_engine); 
                } else {
                    lock_guard<mutex> lock(book_mutex);
                    if (!spot_bids.empty() && !futures_bids.empty()) {
                        double binance_btc_spot = (spot_bids.rbegin()->first + spot_asks.begin()->first) / 2.0;
                        double binance_btc_futures = (futures_bids.rbegin()->first + futures_asks.begin()->first) / 2.0;
                        real_vol_analyzer.update_market_data("Binance", "Bitcoin", binance_btc_spot, binance_btc_futures);
                        real_cross_analyzer.update_asset_price("Binance", "Bitcoin", binance_btc_spot);
                    }
                }
                
                {
                    PERF_TIMER("synthetic_analysis");
                    synthetic_engine.print_mispricing_opportunities();
                }
                
                try {
                    cout << "\n" << string(80, '*') << endl;
                    cout << "*** ADVANCED TRADING STRATEGIES ***" << endl;
                    cout << string(80, '*') << endl;
                    
                    if (multi_leg_engine != nullptr) {
                        multi_leg_engine->print_multi_leg_opportunities();
                    }
                    
                    if (cycle_count % 20 == 0) {
                        lock_guard<mutex> lock(book_mutex);
                        if (!spot_bids.empty() && !spot_asks.empty()) {
                            vector<float> real_bids, real_asks;
                            auto bid_it = spot_bids.rbegin();
                            auto ask_it = spot_asks.begin();
                            
                            for (int i = 0; i < 8 && bid_it != spot_bids.rend() && ask_it != spot_asks.end(); ++i) {
                                real_bids.push_back(static_cast<float>(bid_it->first));
                                real_asks.push_back(static_cast<float>(ask_it->first));
                                ++bid_it; ++ask_it;
                            }
                            
                            if (real_bids.size() >= 4) {
                                auto simd_result = SIMDOptimizer::calculate_arbitrage_batch_simd(real_bids, real_asks, 0.01f);
                                cout << "\n*** SIMD REAL-TIME ANALYSIS ***" << endl;
                                cout << string(60, '=') << endl;
                                cout << "[SIMD] Real Market Data: Processed " << real_bids.size() << " price pairs" << endl;
                                cout << "[SIMD] Micro-opportunities Found: " << simd_result.valid_count << endl;
                                if (simd_result.valid_count > 0) {
                                    cout << "[SIMD] Best Micro-profit: $" << fixed << setprecision(4) << simd_result.profits[0] << " ("
                                         << setprecision(6) << simd_result.roi_percentages[0] << "%)" << endl;
                                }
                                
                                vector<float> spreads;
                                SIMDOptimizer::calculate_spreads_simd(real_bids, real_asks, spreads);
                                if (!spreads.empty()) {
                                    cout << "[SIMD] Real-time Spread Analysis: Avg $"
                                         << fixed << setprecision(4)
                                         << SIMDOptimizer::calculate_mean_simd(spreads) << endl;
                                }
                                cout << string(60, '=') << endl;
                            }
                        }
                    }
                    cout << string(80, '*') << endl;
                } catch (const exception& e) {
                    cout << "[ERROR] Advanced strategies error: " << e.what() << endl;
                }
                
                try {
                    real_vol_analyzer.print_real_volatility_analysis();
                    real_cross_analyzer.print_real_cross_asset_analysis();
                } catch (const exception& e) {
                    cout << "[ERROR] REAL advanced arbitrage strategies error: " << e.what() << endl;
                }
                
                auto now = chrono::high_resolution_clock::now();
                if (chrono::duration_cast<chrono::seconds>(now - last_performance_report).count() >= 30) {
                    PerformanceMonitor::print_performance_report();
                    connection_pool.print_connection_stats();
                    last_performance_report = now;
                }
            }
            
            if (cycle_count % 50 == 0) {
                risk_manager.print_risk_summary();
            }
            
            this_thread::sleep_for(chrono::milliseconds(50));
        } catch (const exception& e) {
            cout << "[ERROR] Main loop error: " << e.what() << endl;
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    
    cout << "[SHUTDOWN] Starting graceful shutdown..." << endl;
    
    try {
        cout << "\n[SHUTDOWN] FINAL PERFORMANCE REPORT:" << endl;
        PerformanceMonitor::print_performance_report();
        connection_pool.print_connection_stats();
        
        if (multi_leg_engine != nullptr) {
            cout << "\n[SHUTDOWN] FINAL MULTI-LEG ANALYSIS:" << endl;
            multi_leg_engine->print_multi_leg_opportunities();
        }
        
        cout << "\n[SHUTDOWN] FINAL SIMD BENCHMARK:" << endl;
        SIMDOptimizer::benchmark_simd_performance();
        
        cout << "\n[SHUTDOWN] FINAL REAL ARBITRAGE ANALYSIS:" << endl;
        real_vol_analyzer.print_real_volatility_analysis();
        real_cross_analyzer.print_real_cross_asset_analysis();
        
    } catch (const exception& e) {
        cout << "[ERROR] Shutdown report error: " << e.what() << endl;
    }
    
    synthetic_engine.stop();
    connection_pool.stop_all_connections();
    PerformanceMonitor::stop_monitoring();
    
    if (multi_leg_engine != nullptr) {
        delete multi_leg_engine;
        multi_leg_engine = nullptr;
    }
    
    cout << "[SHUTDOWN] System shutdown completed successfully." << endl;
    return 0;
}
