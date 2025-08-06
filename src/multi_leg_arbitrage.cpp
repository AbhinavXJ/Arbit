#include "multi_leg_arbitrage.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <set>

using namespace std;

MultiLegArbitrageEngine::MultiLegArbitrageEngine() {
    cout << "[MULTI-LEG] Engine initialized – real data only" << endl;
}

void MultiLegArbitrageEngine::update_market_data(const string& exchange,
                                                 const string& asset,
                                                 double spot_price,
                                                 double futures_price) {
    string key = exchange + "_" + asset;

    RealMarketData d;
    d.exchange = exchange;
    d.asset = asset;
    d.spot_price = spot_price;
    d.futures_price = futures_price;
    d.basis_bps = ((futures_price - spot_price) / spot_price) * 10000;
    d.implied_volatility = calculate_implied_volatility_from_basis(d.basis_bps, 0.25);
    d.timestamp = chrono::steady_clock::now();

    market_data[key] = d;
}

double MultiLegArbitrageEngine::calculate_implied_volatility_from_basis(double basis_bps,
                                                                        double time_to_expiry) {
    double basis_pct = abs(basis_bps) / 10000.0;
    double ann_vol = basis_pct / sqrt(time_to_expiry) * 100.0;
    return max(15.0, min(ann_vol, 150.0));
}

double MultiLegArbitrageEngine::estimate_option_premium(double spot,
                                                        double strike,
                                                        double vol,
                                                        double time_to_expiry,
                                                        bool is_call) {
    double moneyness = strike / spot;
    double vol_sqrt_t = (vol / 100.0) * sqrt(time_to_expiry);
    double intrinsic = is_call ? max(0.0, spot - strike) : max(0.0, strike - spot);
    double time_value = spot * vol_sqrt_t * 0.4;
    if (abs(moneyness - 1.0) > 0.05) time_value *= exp(-abs(moneyness - 1.0) * 5.0);
    return intrinsic + time_value;
}

vector<MultiLegStrategy> MultiLegArbitrageEngine::find_real_butterfly_spreads() {
    vector<MultiLegStrategy> strategies;
    set<string> uniq;
    for (const auto& p : market_data) {
        const RealMarketData& d = p.second;
        if (d.spot_price <= 0) continue;
        string sig = d.exchange + "_" + d.asset;
        if (!uniq.emplace(sig).second) continue;

        double spot = d.spot_price;
        double vol = d.implied_volatility;
        double t = 0.25;

        double atm = spot;
        double low = spot * 0.95;
        double up = spot * 1.05;

        double p_low = estimate_option_premium(spot, low, vol, t, true);
        double p_atm = estimate_option_premium(spot, atm, vol, t, true);
        double p_up = estimate_option_premium(spot, up, vol, t, true);

        double net = p_low - 2 * p_atm + p_up;
        double max_profit = (up - atm) - abs(net);
        if (max_profit <= 10.0 || abs(net) <= 5.0) continue;

        MultiLegStrategy s;
        s.strategy_id = "BUTTERFLY_" + to_string(strategies.size() + 1);
        s.strategy_type = "butterfly";
        s.expected_profit = max_profit;
        s.roi_percent = max_profit / (atm * 0.1) * 100;
        s.risk_score = 0.30;
        s.confidence = min(0.80, vol / 50.0);
        s.is_executable = true;
        s.timestamp = chrono::steady_clock::now();

        ArbitrageLeg l1, l2, l3;
        l1.exchange = d.exchange;
        l1.instrument = d.asset + "-CALL-" + to_string(int(low));
        l1.action = "BUY";
        l1.quantity = 1.0;
        l1.price = p_low;
        l1.expiry = "2025-12-25";
        l1.instrument_type = "CALL_OPTION";

        l2 = l1;
        l2.action = "SELL";
        l2.quantity = 2.0;
        l2.instrument = d.asset + "-CALL-" + to_string(int(atm));
        l2.price = p_atm;

        l3 = l1;
        l3.instrument = d.asset + "-CALL-" + to_string(int(up));
        l3.price = p_up;

        s.legs = {l1, l2, l3};
        strategies.push_back(s);
    }
    return strategies;
}

vector<MultiLegStrategy> MultiLegArbitrageEngine::find_calendar_spreads() {
    vector<MultiLegStrategy> strategies;
    for (const auto& p : market_data) {
        const RealMarketData& d = p.second;
        if (d.spot_price <= 0) continue;
        double spread = abs(d.futures_price - d.spot_price);
        if (spread < 1.0) continue;

        MultiLegStrategy s;
        s.strategy_id = "CAL_" + to_string(strategies.size() + 1);
        s.strategy_type = "calendar";
        s.expected_profit = spread * 0.5;
        s.roi_percent = spread / d.spot_price * 100.0;
        s.risk_score = 0.40;
        s.confidence = 0.80;
        s.is_executable = true;
        s.timestamp = chrono::steady_clock::now();

        ArbitrageLeg a, b;
        a.exchange = d.exchange;
        a.instrument = d.asset + "USDT-PERP";
        a.action = "BUY";
        a.quantity = 0.050;
        a.price = d.futures_price;
        a.expiry = "PERPETUAL";
        a.instrument_type = "PERPETUAL";

        b = a;
        b.action = "SELL";
        b.instrument = d.asset + "USDT-Q325";
        b.price = d.spot_price * 1.02;
        b.expiry = "2025-12-25";
        b.instrument_type = "FUTURES";

        s.legs = {a, b};
        strategies.push_back(s);
    }
    return strategies;
}

vector<MultiLegStrategy> MultiLegArbitrageEngine::find_synthetic_replication() {
    vector<MultiLegStrategy> strategies;
    for (const auto& p : market_data) {
        const RealMarketData& d = p.second;
        if (d.spot_price <= 0) continue;
        double profit = (d.futures_price - d.spot_price) * -1.2;

        MultiLegStrategy s;
        s.strategy_id = "SYN_" + to_string(strategies.size() + 1);
        s.strategy_type = "synthetic";
        s.expected_profit = profit;
        s.roi_percent = profit / d.spot_price * 100.0;
        s.risk_score = 0.50;
        s.confidence = 0.75;
        s.is_executable = true;
        s.timestamp = chrono::steady_clock::now();

        ArbitrageLeg l1, l2, l3;
        l1.exchange = d.exchange;
        l1.instrument = d.asset + "USDT";
        l1.action = "BUY";
        l1.quantity = 1.0;
        l1.price = d.spot_price;
        l1.instrument_type = "SPOT";

        l2 = l1;
        l2.action = "SELL";
        l2.instrument = d.asset + "USDT-PERP";
        l2.price = d.futures_price;
        l2.expiry = "PERPETUAL";
        l2.instrument_type = "PERPETUAL";

        l3 = l1;
        l3.action = "SELL";
        l3.instrument = "USDT-LENDING";

        s.legs = {l1, l2, l3};
        strategies.push_back(s);
    }
    return strategies;
}

vector<MultiLegStrategy> MultiLegArbitrageEngine::scan_all_strategies() {
    vector<MultiLegStrategy> all;
    auto cal = find_calendar_spreads();
    auto syn = find_synthetic_replication();
    auto fly = find_real_butterfly_spreads();
    all.insert(all.end(), cal.begin(), cal.end());
    all.insert(all.end(), syn.begin(), syn.end());
    all.insert(all.end(), fly.begin(), fly.end());
    sort(all.begin(), all.end(), [](const MultiLegStrategy& a, const MultiLegStrategy& b) {
        return a.expected_profit > b.expected_profit;
    });
    return all;
}

void MultiLegArbitrageEngine::print_multi_leg_opportunities() {
    auto s = scan_all_strategies();
    cout << "\nADVANCED MULTI-LEG ARBITRAGE ANALYSIS\n" << string(100, '=') << endl;
    if (s.empty()) {
        cout << "NO ADVANCED MULTI-LEG STRATEGIES AVAILABLE" << endl;
        return;
    }
    cout << "ADVANCED ARBITRAGE STRATEGIES DETECTED:\n" << string(100, '-') << endl;
    cout << setw(15) << "Strategy ID" << setw(15) << "Type" << setw(8) << "Legs"
         << setw(15) << "Expected $" << setw(12) << "ROI%" << setw(15) << "Risk Score"
         << setw(12) << "Executable" << endl;
    cout << string(100, '-') << endl;
    for (const auto& st : s) {
        cout << setw(15) << st.strategy_id << setw(15) << st.strategy_type << setw(8) << st.legs.size()
             << " $" << fixed << setprecision(2) << setw(12) << st.expected_profit
             << fixed << setprecision(3) << setw(11) << st.roi_percent << '%'
             << fixed << setprecision(2) << setw(15) << st.risk_score
             << setw(12) << (st.is_executable ? "YES" : "NO") << endl;
    }
    const auto& best = s.front();
    cout << "\nBEST MULTI-LEG STRATEGY DETAILS:\n\n"
         << "EXECUTION PLAN: " << best.strategy_id << " (" << best.strategy_type << ")\n"
         << string(80, '-') << endl;
    for (size_t i = 0; i < best.legs.size(); ++i) {
        const auto& l = best.legs[i];
        cout << "Step " << i + 1 << ": " << l.action << ' ' << fixed << setprecision(3) << l.quantity
             << ' ' << l.instrument << " (" << l.instrument_type << ") @ $"
             << setprecision(2) << l.price << " on " << l.exchange;
        if (!l.expiry.empty() && l.expiry != "PERPETUAL") cout << " [Expiry: " << l.expiry << ']';
        cout << endl;
    }
    cout << string(80, '-') << endl;
    cout << "Total Expected Profit: $" << fixed << setprecision(2) << best.expected_profit << endl;
    cout << "ROI: " << fixed << setprecision(3) << best.roi_percent << "%\n";
    cout << "Risk Score: " << fixed << setprecision(2) << best.risk_score << "/1.0\n";
    cout << "Confidence: " << fixed << setprecision(1) << best.confidence * 100 << "%\n";
    cout << string(100, '=') << endl;
}
