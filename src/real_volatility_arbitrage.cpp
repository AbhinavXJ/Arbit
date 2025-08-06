#include "real_volatility_arbitrage.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace std;

RealVolatilityArbitrage::RealVolatilityArbitrage() {
    cout << "[REAL VOLATILITY] Engine initialized - NO simulation, real data only" << endl;
    cout << "[VOLATILITY] FIXED Threshold: ±" << min_vol_spread_bps << " bps (DIRECT BASIS DETECTION)" << endl;
}

string RealVolatilityArbitrage::generate_key(const string& exchange, const string& asset) {
    return exchange + "_" + asset;
}

void RealVolatilityArbitrage::update_market_data(const string& exchange, const string& asset,
                                               double spot_price, double futures_price) {
    lock_guard<mutex> lock(volatility_mutex);
    
    if (spot_price <= 0 || futures_price <= 0) return;
    
    string key = generate_key(exchange, asset);
    
    MarketDataPoint point;
    point.spot_price = spot_price;
    point.futures_price = futures_price;
    point.basis_bps = ((futures_price - spot_price) / spot_price) * 10000; // Basis in bps
    point.timestamp = chrono::steady_clock::now();
    
    auto& history = market_history[key];
    history.push_back(point);
    
    if (history.size() > static_cast<size_t>(history_window)) {
        history.erase(history.begin());
    }
    
    if (history.size() >= 3) {
        VolatilityIndicator indicator;
        indicator.realized_volatility = calculate_realized_volatility(history);
        indicator.basis_implied_volatility = abs(point.basis_bps);
        indicator.volatility_risk_premium = point.basis_bps / 100.0; // For display only
        indicator.confidence_score = min(0.95, history.size() / 10.0);
        indicator.timestamp = chrono::steady_clock::now();
        
        current_indicators[key] = indicator;
        
        // cout << "debugging" << key << " - Basis: " << fixed << setprecision(1) 
        //      << point.basis_bps << " bps, History: " << history.size() << " points" << endl;
    }
}

double RealVolatilityArbitrage::calculate_realized_volatility(const vector<MarketDataPoint>& history) {
    if (history.size() < 3) return 0.0; 
    
    vector<double> returns;
    for (size_t i = 1; i < history.size(); i++) {
        if (history[i-1].spot_price > 0) {
            double return_rate = log(history[i].spot_price / history[i-1].spot_price);
            returns.push_back(return_rate);
        }
    }
    
    if (returns.empty()) return 0.0;
    
    double mean = accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;
    for (double ret : returns) {
        variance += pow(ret - mean, 2);
    }
    variance /= (returns.size() - 1);
    
    double vol = sqrt(variance) * sqrt(525600) * 100; // 525600 minutes per year
    return min(vol, 200.0); // Cap at 200%
}

double RealVolatilityArbitrage::calculate_basis_implied_volatility(const vector<MarketDataPoint>& history) {
    if (history.size() < 3) return 0.0; // REDUCED from 5 to 3
    
    vector<double> basis_changes;
    for (size_t i = 1; i < history.size(); i++) {
        double basis_change = history[i].basis_bps - history[i-1].basis_bps;
        basis_changes.push_back(basis_change);
    }
    
    if (basis_changes.empty()) return 0.0;
    
    double mean = accumulate(basis_changes.begin(), basis_changes.end(), 0.0) / basis_changes.size();
    double variance = 0.0;
    for (double change : basis_changes) {
        variance += pow(change - mean, 2);
    }
    variance /= (basis_changes.size() - 1);
    
    double basis_vol = sqrt(variance) * sqrt(525600); // Annualized
    double implied_vol_proxy = basis_vol * 0.1; // Scale factor for crypto markets
    
    return min(implied_vol_proxy, 150.0);
}

double RealVolatilityArbitrage::calculate_volatility_risk_premium(double realized_vol, double implied_vol) {
    if (realized_vol <= 0 || implied_vol <= 0) return 0.0;
    return implied_vol - realized_vol;
}

vector<RealVolatilityOpportunity> RealVolatilityArbitrage::scan_real_volatility_opportunities() {
    lock_guard<mutex> lock(volatility_mutex);
    vector<RealVolatilityOpportunity> opportunities;
    
    
    for (const auto& entry : current_indicators) {
        const string& key = entry.first;
        const VolatilityIndicator& indicator = entry.second;
        
        size_t underscore_pos = key.find('_');
        if (underscore_pos == string::npos) continue;
        string exchange = key.substr(0, underscore_pos);
        string asset = key.substr(underscore_pos + 1);
        
        if (market_history.find(key) == market_history.end() || market_history[key].empty()) continue;
        
        const MarketDataPoint& current = market_history[key].back();
        
        double vol_spread_bps = abs(current.basis_bps);
        
       
        
        if (vol_spread_bps >= min_vol_spread_bps && vol_spread_bps <= max_vol_spread_bps) {
           
            
            RealVolatilityOpportunity opp;
            opp.exchange = exchange;
            opp.asset = asset;
            opp.current_spot_price = current.spot_price;
            opp.current_futures_price = current.futures_price;
            opp.basis_bps = current.basis_bps;
            opp.realized_vol = indicator.realized_volatility;
            opp.implied_vol_proxy = indicator.basis_implied_volatility;
            opp.volatility_spread = indicator.volatility_risk_premium;
            
            if (current.basis_bps > 0) {
                opp.strategy = "Futures OVERPRICED: SELL futures + BUY spot";
            } else {
                opp.strategy = "Futures UNDERPRICED: BUY futures + SELL spot";
            }
            
            opp.expected_profit = vol_spread_bps * current.spot_price * 0.00005; // $5 per 100 bps
            opp.confidence = indicator.confidence_score;
            opp.is_executable = vol_spread_bps >= min_vol_spread_bps;
            
            opportunities.push_back(opp);
        } else {
            
        }
    }
    
    
    sort(opportunities.begin(), opportunities.end(),
        [](const RealVolatilityOpportunity& a, const RealVolatilityOpportunity& b) {
            return abs(a.basis_bps) > abs(b.basis_bps);
        });
    
    return opportunities;
}

void RealVolatilityArbitrage::print_real_volatility_analysis() {
    auto opportunities = scan_real_volatility_opportunities();
    
    cout << "\n REAL VOLATILITY ARBITRAGE ANALYSIS" << endl;
    cout << string(100, '=') << endl;
    
    if (opportunities.empty()) {
        cout << " NO SIGNIFICANT VOLATILITY MISPRICINGS DETECTED" << endl;
        cout << " Futures basis and realized volatilities are aligned" << endl;
        cout << " FIXED Monitoring threshold: ±" << fixed << setprecision(1) << min_vol_spread_bps << " bps" << endl;
        
        cout << "\n CURRENT BASIS SPREADS:" << endl;
        lock_guard<mutex> lock(volatility_mutex);
        for (const auto& entry : market_history) {
            if (!entry.second.empty()) {
                const auto& latest = entry.second.back();
                cout << "  " << entry.first << ": " << fixed << setprecision(1) 
                     << latest.basis_bps << " bps (Spot: $" << setprecision(2) 
                     << latest.spot_price << ", Futures: $" << latest.futures_price << ")" << endl;
            }
        }
    } else {
        cout << " " << opportunities.size() << " REAL VOLATILITY OPPORTUNITIES DETECTED:" << endl;
        cout << string(120, '-') << endl;
        cout << setw(10) << "Exchange" << setw(12) << "Asset"
             << setw(12) << "Spot $" << setw(12) << "Futures $"
             << setw(12) << "Basis bps" << setw(12) << "Real Vol%"
             << setw(12) << "Impl Vol%" << setw(18) << "Strategy"
             << setw(12) << "Profit $" << endl;
        cout << string(120, '-') << endl;
        
        for (const auto& opp : opportunities) {
            cout << setw(10) << opp.exchange
                 << setw(12) << opp.asset
                 << " $" << fixed << setprecision(2) << setw(10) << opp.current_spot_price
                 << " $" << fixed << setprecision(2) << setw(10) << opp.current_futures_price
                 << fixed << setprecision(1) << setw(11) << opp.basis_bps << " bps"
                 << fixed << setprecision(1) << setw(11) << opp.realized_vol << "%"
                 << fixed << setprecision(1) << setw(11) << opp.implied_vol_proxy << "%"
                 << setw(18) << opp.strategy.substr(0, 17)
                 << " $" << fixed << setprecision(2) << setw(10) << opp.expected_profit << endl;
        }
        
        cout << "\n REAL VOLATILITY TRADING STRATEGIES:" << endl;
        for (size_t i = 0; i < min(opportunities.size(), size_t(3)); i++) {
            const auto& opp = opportunities[i];
            cout << "\n[VOLATILITY " << (i+1) << "] " << opp.exchange << " " << opp.asset << endl;
            cout << " Spot: $" << fixed << setprecision(2) << opp.current_spot_price 
                 << " | Futures: $" << opp.current_futures_price << endl;
            cout << " Basis: " << fixed << setprecision(1) << opp.basis_bps << " bps" << endl;
            
            if (opp.basis_bps > 0) {
                cout << " Analysis: Futures trading at PREMIUM to spot" << endl;
                cout << " Strategy: SELL " << opp.exchange << " " << opp.asset << " futures + BUY spot" << endl;
                cout << " Expected: Profit as futures converge down to spot price" << endl;
            } else {
                cout << " Analysis: Futures trading at DISCOUNT to spot" << endl;
                cout << " Strategy: BUY " << opp.exchange << " " << opp.asset << " futures + SELL spot" << endl;
                cout << " Expected: Profit as futures converge up to spot price" << endl;
            }
            
            cout << " Realized Vol: " << fixed << setprecision(1) << opp.realized_vol << "%" << endl;
            cout << " Implied Vol (basis): " << fixed << setprecision(1) << opp.implied_vol_proxy << "%" << endl;
            cout << " Vol Spread: " << fixed << setprecision(1) << opp.volatility_spread << "%" << endl;
            cout << " Expected: $" << fixed << setprecision(2) << opp.expected_profit << endl;
            cout << " Confidence: " << fixed << setprecision(1) << (opp.confidence * 100) << "%" << endl;
        }
    }
    cout << string(100, '=') << endl;
}
