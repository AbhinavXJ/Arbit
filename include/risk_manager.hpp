#ifndef RISK_MANAGER_HPP
#define RISK_MANAGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <atomic>

using namespace std;

struct Position {
    string exchange;
    string instrument;
    string type; 
    double entry_price;
    double current_price;
    double quantity;
    double unrealized_pnl;
    double realized_pnl;
    chrono::steady_clock::time_point entry_time;
    bool is_active;
    string strategy_id; 
};

struct TradeSignal {
    string exchange;
    string instrument;
    string action; 
    double price;
    double expected_profit;
    double risk_amount;
    string strategy_type; 
    double confidence_score;
};

struct RiskMetrics {
    double total_capital;
    double available_capital;
    double total_exposure;
    double daily_pnl;
    double max_drawdown;
    double win_rate;
    int total_trades;
    int winning_trades;
    double avg_profit_per_trade;
    double sharpe_ratio;
};

struct RiskConfig {
    // Position sizing rules
    double max_risk_per_trade = 0.01;          // 1% max risk per trade
    double max_total_exposure = 0.20;          // 20% max total exposure
    double max_single_position = 0.05;        // 5% max single position
    
    // Risk management
    double stop_loss_percent = 0.02;           // 2% stop loss
    double take_profit_percent = 0.05;         // 5% take profit
    double max_daily_loss = 0.05;              // 5% max daily loss
    
    // Capital management
    double initial_capital = 10000.0;          // $10k starting capital
    double min_trade_size = 0.001;             // Minimum 0.001 BTC
    double max_leverage = 3.0;                 // 3x max leverage
    
    // Opportunity filters
    double min_profit_threshold = 0.0005;      // 0.05% minimum profit
    double min_confidence_score = 0.7;         // 70% confidence minimum
    
    bool enable_auto_trading = false;          // Manual approval required
    bool enable_stop_loss = true;
    bool enable_position_sizing = true;
};

class RiskManager {
private:
    RiskConfig config;
    RiskMetrics current_metrics;
    vector<Position> positions;
    vector<TradeSignal> pending_signals;
    mutable mutex risk_mutex;
    atomic<bool> emergency_stop{false};
    
    // Internal calculations
    double calculate_position_size(const TradeSignal& signal);
    double calculate_risk_amount(double position_size, double entry_price, double stop_loss_price);
    double calculate_max_position_size(double available_capital, double risk_percent);
    bool validate_trade_limits(const TradeSignal& signal, double position_size);
    void update_position_pnl(Position& position, double current_price);
    void check_stop_loss_conditions();
    void update_risk_metrics();
    
public:
    RiskManager(const RiskConfig& cfg = RiskConfig{});
    ~RiskManager();
    
    bool evaluate_opportunity(const TradeSignal& signal, double& recommended_size);
    vector<TradeSignal> get_approved_trades();
    
    bool open_position(const TradeSignal& signal, double actual_fill_price);
    bool close_position(const string& position_id, double exit_price);
    void update_positions(const unordered_map<string, double>& current_prices);
    
    RiskMetrics get_current_metrics();
    bool is_emergency_stop_triggered();
    void trigger_emergency_stop(const string& reason);
    
    void update_config(const RiskConfig& new_config);
    void set_current_capital(double capital);
    
    void print_risk_summary();
    void print_positions();
    void print_performance_metrics();
    
    void process_arbitrage_opportunity(const string& exchange, const string& instrument, 
                                     double profit, double confidence, const string& strategy_type);
};

extern RiskManager* global_risk_manager;

#endif
