#include "risk_manager.hpp"
#include "book_storage.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

using namespace std;

RiskManager* global_risk_manager = nullptr;

static double mid_price_from_books(const std::string& exch, const std::string& instr) {
    lock_guard<mutex> lock(book_mutex);
    if (exch == "Binance" && instr == "Bitcoin") {
        if (spot_bids.empty() || spot_asks.empty()) return 0.0;
        return (spot_bids.rbegin()->first + spot_asks.begin()->first) / 2.0;
    }
    if (exch == "Binance" && instr == "Bitcoin_Futures") {
        if (futures_bids.empty() || futures_asks.empty()) return 0.0;
        return (futures_bids.rbegin()->first + futures_asks.begin()->first) / 2.0;
    }
    if (exch == "Binance" && instr == "Ethereum") {
        if (eth_spot_bids.empty() || eth_spot_asks.empty()) return 0.0;
        return (eth_spot_bids.rbegin()->first + eth_spot_asks.begin()->first) / 2.0;
    }
    if (exch == "Binance" && instr == "Ethereum_Futures") {
        if (eth_futures_bids.empty() || eth_futures_asks.empty()) return 0.0;
        return (eth_futures_bids.rbegin()->first + eth_futures_asks.begin()->first) / 2.0;
    }
    if (exch == "Bybit" && instr == "Bitcoin") {
        if (bybit_spot_bids.empty() || bybit_spot_asks.empty()) return 0.0;
        return (bybit_spot_bids.rbegin()->first + bybit_spot_asks.begin()->first) / 2.0;
    }
    if (exch == "Bybit" && instr == "Bitcoin_Futures") {
        if (bybit_futures_bids.empty() || bybit_futures_asks.empty()) return 0.0;
        return (bybit_futures_bids.rbegin()->first + bybit_futures_asks.begin()->first) / 2.0;
    }
    if (exch == "Bybit" && instr == "Ethereum") {
        if (eth_bybit_spot_bids.empty() || eth_bybit_spot_asks.empty()) return 0.0;
        return (eth_bybit_spot_bids.rbegin()->first + eth_bybit_spot_asks.begin()->first) / 2.0;
    }
    if (exch == "Bybit" && instr == "Ethereum_Futures") {
        if (eth_bybit_futures_bids.empty() || eth_bybit_futures_asks.empty()) return 0.0;
        return (eth_bybit_futures_bids.rbegin()->first + eth_bybit_futures_asks.begin()->first) / 2.0;
    }
    if (exch == "OKX" && instr == "Bitcoin") {
        if (okx_spot_bids.empty() || okx_spot_asks.empty()) return 0.0;
        return (okx_spot_bids.rbegin()->first + okx_spot_asks.begin()->first) / 2.0;
    }
    if (exch == "OKX" && instr == "Bitcoin_Futures") {
        if (okx_futures_bids.empty() || okx_futures_asks.empty()) return 0.0;
        return (okx_futures_bids.rbegin()->first + okx_futures_asks.begin()->first) / 2.0;
    }
    if (exch == "OKX" && instr == "Ethereum") {
        if (eth_okx_spot_bids.empty() || eth_okx_spot_asks.empty()) return 0.0;
        return (eth_okx_spot_bids.rbegin()->first + eth_okx_spot_asks.begin()->first) / 2.0;
    }
    if (exch == "OKX" && instr == "Ethereum_Futures") {
        if (eth_okx_futures_bids.empty() || eth_okx_futures_asks.empty()) return 0.0;
        return (eth_okx_futures_bids.rbegin()->first + eth_okx_futures_asks.begin()->first) / 2.0;
    }
    return 0.0;
}

RiskManager::RiskManager(const RiskConfig& cfg) : config(cfg) {
    current_metrics.total_capital = config.initial_capital;
    current_metrics.available_capital = config.initial_capital;
    current_metrics.total_exposure = 0.0;
    current_metrics.daily_pnl = 0.0;
    current_metrics.max_drawdown = 0.0;
    current_metrics.total_trades = 0;
    current_metrics.winning_trades = 0;
    global_risk_manager = this;
    cout << "[RISK MANAGER] Initialized with $" << fixed << setprecision(2)
         << config.initial_capital << " capital" << endl;
}

RiskManager::~RiskManager() {
    global_risk_manager = nullptr;
}

double RiskManager::calculate_position_size(const TradeSignal& signal) {
    if (!config.enable_position_sizing) {
        return config.min_trade_size;
    }
    double risk_amount = current_metrics.total_capital * config.max_risk_per_trade;
    double stop_loss_price = signal.price * (1.0 - config.stop_loss_percent);
    double risk_per_unit = abs(signal.price - stop_loss_price);
    if (risk_per_unit > 0) {
        double position_size = risk_amount / risk_per_unit;
        double max_position = current_metrics.available_capital * config.max_single_position;
        double max_size_by_capital = max_position / signal.price;
        position_size = min(position_size, max_size_by_capital);
        position_size = max(position_size, config.min_trade_size);
        return position_size;
    }
    return config.min_trade_size;
}

bool RiskManager::evaluate_opportunity(const TradeSignal& signal, double& recommended_size) {
    lock_guard<mutex> lock(risk_mutex);
    if (emergency_stop) {
        cout << "[RISK MANAGER] Emergency stop active - rejecting trade" << endl;
        return false;
    }
    if (signal.expected_profit < config.min_profit_threshold) {
        return false;
    }
    if (signal.confidence_score < config.min_confidence_score) {
        return false;
    }
    if (current_metrics.daily_pnl < -config.max_daily_loss * current_metrics.total_capital) {
        cout << "[RISK MANAGER] Daily loss limit reached - rejecting trade" << endl;
        return false;
    }
    recommended_size = calculate_position_size(signal);
    if (!validate_trade_limits(signal, recommended_size)) {
        return false;
    }
    double new_exposure = current_metrics.total_exposure + (recommended_size * signal.price);
    if (new_exposure > config.max_total_exposure * current_metrics.total_capital) {
        cout << "[RISK MANAGER] Total exposure limit would be exceeded - reducing size" << endl;
        double max_additional = (config.max_total_exposure * current_metrics.total_capital) - current_metrics.total_exposure;
        recommended_size = max_additional / signal.price;
        if (recommended_size < config.min_trade_size) {
            return false;
        }
    }
    cout << "[RISK MANAGER] Trade approved: " << signal.action << " "
         << fixed << setprecision(3) << recommended_size << " " << signal.instrument
         << " @ $" << setprecision(2) << signal.price << endl;
    return true;
}

bool RiskManager::validate_trade_limits(const TradeSignal& signal, double position_size) {
    if (position_size < config.min_trade_size) {
        return false;
    }
    double trade_value = position_size * signal.price;
    if (trade_value > current_metrics.available_capital) {
        return false;
    }
    if (trade_value > config.max_single_position * current_metrics.total_capital) {
        return false;
    }
    return true;
}

void RiskManager::process_arbitrage_opportunity(const string& exchange, const string& instrument,
                                               double profit, double confidence, const string& strategy_type) {
    TradeSignal signal;
    signal.exchange = exchange;
    signal.instrument = instrument;
    signal.action = profit > 0 ? "BUY" : "SELL";
    signal.expected_profit = abs(profit);
    signal.confidence_score = confidence;
    signal.strategy_type = strategy_type;
    signal.price = mid_price_from_books(exchange, instrument);
    if (signal.price == 0.0) return;
    double recommended_size;
    if (evaluate_opportunity(signal, recommended_size)) {
        lock_guard<mutex> lock(risk_mutex);
        signal.risk_amount = recommended_size * signal.price * config.max_risk_per_trade;
        pending_signals.push_back(signal);
    }
}

void RiskManager::update_risk_metrics() {
    current_metrics.total_exposure = 0.0;
    double total_unrealized_pnl = 0.0;
    for (const auto& position : positions) {
        if (position.is_active) {
            current_metrics.total_exposure += abs(position.quantity * position.current_price);
            total_unrealized_pnl += position.unrealized_pnl;
        }
    }
    current_metrics.available_capital = current_metrics.total_capital + total_unrealized_pnl - current_metrics.total_exposure;
    if (current_metrics.total_trades > 0) {
        current_metrics.win_rate = static_cast<double>(current_metrics.winning_trades) / current_metrics.total_trades;
    }
    if (current_metrics.total_trades > 0) {
        current_metrics.avg_profit_per_trade = current_metrics.daily_pnl / current_metrics.total_trades;
    }
}

void RiskManager::print_risk_summary() {
    lock_guard<mutex> lock(risk_mutex);
    update_risk_metrics();
    cout << "\nRISK MANAGEMENT SUMMARY" << endl;
    cout << string(80, '=') << endl;
    cout << "CAPITAL OVERVIEW:" << endl;
    cout << "  Total Capital: $" << fixed << setprecision(2) << current_metrics.total_capital << endl;
    cout << "  Available: $" << fixed << setprecision(2) << current_metrics.available_capital << endl;
    cout << "  Total Exposure: $" << fixed << setprecision(2) << current_metrics.total_exposure
         << " (" << fixed << setprecision(1) << (current_metrics.total_exposure / current_metrics.total_capital * 100) << "%)" << endl;
    cout << "  Daily P&L: $" << fixed << setprecision(2) << current_metrics.daily_pnl << endl;
    cout << "\nRISK LIMITS & USAGE:" << endl;
    cout << "  Max Risk/Trade: " << fixed << setprecision(1) << (config.max_risk_per_trade * 100) << "% ($"
         << setprecision(2) << (current_metrics.total_capital * config.max_risk_per_trade) << ")" << endl;
    cout << "  Max Exposure: " << fixed << setprecision(1) << (config.max_total_exposure * 100) << "% ($"
         << setprecision(2) << (current_metrics.total_capital * config.max_total_exposure) << ")" << endl;
    cout << "  Current Usage: " << fixed << setprecision(1)
         << (current_metrics.total_exposure / (current_metrics.total_capital * config.max_total_exposure) * 100) << "%" << endl;
    cout << "\nPERFORMANCE METRICS:" << endl;
    cout << "  Total Trades: " << current_metrics.total_trades << endl;
    cout << "  Win Rate: " << fixed << setprecision(1) << (current_metrics.win_rate * 100) << "%" << endl;
    cout << "  Avg Profit: $" << fixed << setprecision(2) << current_metrics.avg_profit_per_trade << endl;
    cout << "  Active Positions: " << count_if(positions.begin(), positions.end(),
                                               [](const Position& p) { return p.is_active; }) << endl;
    cout << "\nRISK STATUS:" << endl;
    bool risk_ok = true;
    if (current_metrics.total_exposure > config.max_total_exposure * current_metrics.total_capital) {
        cout << "  EXPOSURE LIMIT EXCEEDED" << endl;
        risk_ok = false;
    }
    if (current_metrics.daily_pnl < -config.max_daily_loss * current_metrics.total_capital) {
        cout << "  DAILY LOSS LIMIT EXCEEDED" << endl;
        risk_ok = false;
    }
    if (emergency_stop) {
        cout << "  EMERGENCY STOP ACTIVE" << endl;
        risk_ok = false;
    }
    if (risk_ok) {
        cout << "  ALL RISK PARAMETERS WITHIN LIMITS" << endl;
    }
    cout << string(80, '=') << endl;
}

void RiskManager::print_positions() {
    lock_guard<mutex> lock(risk_mutex);
    auto active_positions = positions;
    active_positions.erase(remove_if(active_positions.begin(), active_positions.end(),
                                   [](const Position& p) { return !p.is_active; }),
                          active_positions.end());
    if (active_positions.empty()) {
        cout << "\nNo active positions" << endl;
        return;
    }
    cout << "\nACTIVE POSITIONS" << endl;
    cout << string(80, '-') << endl;
    cout << setw(10) << "Exchange" << setw(12) << "Instrument" << setw(8) << "Type"
         << setw(10) << "Quantity" << setw(12) << "Entry $" << setw(12) << "Current $"
         << setw(10) << "P&L $" << endl;
    cout << string(80, '-') << endl;
    for (const auto& pos : active_positions) {
        cout << setw(10) << pos.exchange << setw(12) << pos.instrument << setw(8) << pos.type
             << setw(10) << fixed << setprecision(3) << pos.quantity
             << setw(12) << fixed << setprecision(2) << pos.entry_price
             << setw(12) << fixed << setprecision(2) << pos.current_price
             << setw(10) << fixed << setprecision(2) << pos.unrealized_pnl << endl;
    }
    cout << string(80, '-') << endl;
}

RiskMetrics RiskManager::get_current_metrics() {
    lock_guard<mutex> lock(risk_mutex);
    update_risk_metrics();
    return current_metrics;
}

void RiskManager::set_current_capital(double capital) {
    lock_guard<mutex> lock(risk_mutex);
    current_metrics.total_capital = capital;
    cout << "[RISK MANAGER] Capital updated to $" << fixed << setprecision(2) << capital << endl;
}

bool RiskManager::is_emergency_stop_triggered() {
    return emergency_stop;
}

void RiskManager::trigger_emergency_stop(const string& reason) {
    emergency_stop = true;
    cout << "[RISK MANAGER] EMERGENCY STOP TRIGGERED: " << reason << endl;
}

vector<TradeSignal> RiskManager::get_approved_trades() {
    lock_guard<mutex> lock(risk_mutex);
    return pending_signals;
}

bool RiskManager::open_position(const TradeSignal& signal, double actual_fill_price) {
    lock_guard<mutex> lock(risk_mutex);
    Position pos;
    pos.exchange = signal.exchange;
    pos.instrument = signal.instrument;
    pos.type = signal.action;
    pos.entry_price = actual_fill_price;
    pos.current_price = actual_fill_price;
    pos.quantity = 1.0;
    pos.unrealized_pnl = 0.0;
    pos.realized_pnl = 0.0;
    pos.entry_time = chrono::steady_clock::now();
    pos.is_active = true;
    pos.strategy_id = signal.strategy_type;
    positions.push_back(pos);
    current_metrics.total_trades++;
    return true;
}

bool RiskManager::close_position(const string& position_id, double exit_price) {
    lock_guard<mutex> lock(risk_mutex);
    for (auto& pos : positions) {
        if (pos.strategy_id == position_id && pos.is_active) {
            pos.is_active = false;
            pos.realized_pnl = (exit_price - pos.entry_price) * pos.quantity;
            current_metrics.daily_pnl += pos.realized_pnl;
            if (pos.realized_pnl > 0) {
                current_metrics.winning_trades++;
            }
            return true;
        }
    }
    return false;
}

void RiskManager::update_positions(const unordered_map<string, double>& current_prices) {
    lock_guard<mutex> lock(risk_mutex);
    for (auto& pos : positions) {
        if (pos.is_active) {
            auto it = current_prices.find(pos.instrument);
            if (it != current_prices.end()) {
                pos.current_price = it->second;
                pos.unrealized_pnl = (pos.current_price - pos.entry_price) * pos.quantity;
            }
        }
    }
}

void RiskManager::update_config(const RiskConfig& new_config) {
    lock_guard<mutex> lock(risk_mutex);
    config = new_config;
}

void RiskManager::print_performance_metrics() {
    lock_guard<mutex> lock(risk_mutex);
    update_risk_metrics();
    cout << "\nPERFORMANCE METRICS DETAIL" << endl;
    cout << string(50, '-') << endl;
    cout << "Total Trades: " << current_metrics.total_trades << endl;
    cout << "Winning Trades: " << current_metrics.winning_trades << endl;
    cout << "Win Rate: " << fixed << setprecision(1) << (current_metrics.win_rate * 100) << "%" << endl;
    cout << "Total P&L: $" << fixed << setprecision(2) << current_metrics.daily_pnl << endl;
    cout << "Average per Trade: $" << fixed << setprecision(2) << current_metrics.avg_profit_per_trade << endl;
    cout << string(50, '-') << endl;
}
