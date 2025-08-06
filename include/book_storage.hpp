#ifndef BOOK_STORAGE_HPP
#define BOOK_STORAGE_HPP

#include <map>
#include <string>
#include <mutex>
#include <chrono>
#include <iostream>
#include <iomanip>

using namespace std;

struct OrderbookTimestamp {
    chrono::steady_clock::time_point last_update;
    
    bool is_fresh() const {
        auto now = chrono::steady_clock::now();
        auto age = chrono::duration_cast<chrono::seconds>(now - last_update).count();
        return age < 30;
    }
    
    int age_seconds() const {
        auto now = chrono::steady_clock::now();
        return chrono::duration_cast<chrono::seconds>(now - last_update).count();
    }
};

extern map<double, string> spot_bids;
extern map<double, string> spot_asks;

extern map<double, string> futures_bids;
extern map<double, string> futures_asks;
extern OrderbookTimestamp binance_spot_timestamp;
extern OrderbookTimestamp binance_futures_timestamp;

extern map<double, string> bybit_spot_bids;
extern map<double, string> bybit_spot_asks;
extern map<double, string> bybit_futures_bids;
extern map<double, string> bybit_futures_asks;

extern OrderbookTimestamp bybit_spot_timestamp;
extern OrderbookTimestamp bybit_futures_timestamp;

extern map<double, string> okx_spot_bids;
extern map<double, string> okx_spot_asks;
extern map<double, string> okx_futures_bids;
extern map<double, string> okx_futures_asks;
extern OrderbookTimestamp okx_spot_timestamp;
extern OrderbookTimestamp okx_futures_timestamp;

extern map<double, string> eth_spot_bids;
extern map<double, string> eth_spot_asks;
extern map<double, string> eth_futures_bids;

extern map<double, string> eth_futures_asks;
extern OrderbookTimestamp eth_binance_spot_timestamp;
extern OrderbookTimestamp eth_binance_futures_timestamp;

extern map<double, string> eth_bybit_spot_bids;
extern map<double, string> eth_bybit_spot_asks;

extern map<double, string> eth_bybit_futures_bids;
extern map<double, string> eth_bybit_futures_asks;
extern OrderbookTimestamp eth_bybit_spot_timestamp;
extern OrderbookTimestamp eth_bybit_futures_timestamp;

extern map<double, string> eth_okx_spot_bids;
extern map<double, string> eth_okx_spot_asks;

extern map<double, string> eth_okx_futures_bids;
extern map<double, string> eth_okx_futures_asks;
extern OrderbookTimestamp eth_okx_spot_timestamp;
extern OrderbookTimestamp eth_okx_futures_timestamp;

extern mutex book_mutex;

void print_all_books();
void print_debug_orderbook();
void print_top_orderbook_levels();

#endif
