#ifndef FAST_JSON_PARSER_HPP
#define FAST_JSON_PARSER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <cstdlib>

using namespace std;

class FastJsonParser {
private:
    static thread_local vector<string> price_buffer;
    static thread_local vector<string> quantity_buffer;
    static thread_local unordered_map<string, string> field_cache;
    
    static double fast_strtod(const string& str) {
        if (str.empty()) return 0.0;
        return strtod(str.c_str(), nullptr);
    }
    
    static const char* find_field_simple(const char* json, size_t length, const char* field) {
        if (!json || !field || length == 0) return nullptr;
        
        string search_pattern = "\"" + string(field) + "\"";
        const char* result = strstr(json, search_pattern.c_str());
        return result;
    }
    
    static string extract_string_value(const char* start, const char* end) {
        if (!start || !end || start >= end) return "";
        
        const char* quote_start = strchr(start, '"');
        if (!quote_start || quote_start >= end) return "";
        quote_start++; 
        
        const char* quote_end = strchr(quote_start, '"');
        if (!quote_end || quote_end >= end) return "";
        
        return string(quote_start, quote_end - quote_start);
    }
    
    static vector<pair<double, double>> extract_price_quantity_array(const char* json, const char* field_name) {
        vector<pair<double, double>> result;
        
        const char* field_pos = strstr(json, field_name);
        if (!field_pos) return result;
        
        const char* array_start = strchr(field_pos, '[');
        if (!array_start) return result;
        array_start++; // Skip '['
        
        const char* array_end = strchr(array_start, ']');
        if (!array_end) return result;
        
        const char* current = array_start;
        while (current < array_end) {
            const char* sub_array_start = strchr(current, '[');
            if (!sub_array_start || sub_array_start >= array_end) break;
            sub_array_start++; // Skip '['
            
            const char* sub_array_end = strchr(sub_array_start, ']');
            if (!sub_array_end || sub_array_end >= array_end) break;
            
            const char* comma = strchr(sub_array_start, ',');
            if (comma && comma < sub_array_end) {
                string price_str = extract_string_value(sub_array_start, comma);
                string qty_str = extract_string_value(comma + 1, sub_array_end);
                
                if (!price_str.empty() && !qty_str.empty()) {
                    double price = fast_strtod(price_str);
                    double quantity = fast_strtod(qty_str);
                    if (price > 0.0 && quantity > 0.0) {
                        result.emplace_back(price, quantity);
                    }
                }
            }
            
            current = sub_array_end + 1;
        }
        
        return result;
    }
    
public:
    struct OrderbookUpdate {
        vector<pair<double, double>> bids;
        vector<pair<double, double>> asks;
        string exchange;
        string symbol;
        uint64_t timestamp;
        bool is_snapshot;
        
        OrderbookUpdate() : timestamp(0), is_snapshot(false) {}
    };
    
    static bool parse_orderbook(const string& json_data, OrderbookUpdate& update) {
        if (json_data.empty()) return false;
        
        const char* json = json_data.c_str();
        
        if (json_data.find("\"b\":") != string::npos && json_data.find("\"a\":") != string::npos) {
            return parse_binance_format(json_data, update);
        } else if (json_data.find("\"data\":") != string::npos) {
            if (json_data.find("\"topic\":") != string::npos) {
                return parse_bybit_format(json_data, update);
            } else {
                return parse_okx_format(json_data, update);
            }
        }
        
        return false;
    }
    
    static bool parse_binance_format(const string& json, OrderbookUpdate& update) {
        const char* json_cstr = json.c_str();
        
        update.bids = extract_price_quantity_array(json_cstr, "\"b\":");
        
        update.asks = extract_price_quantity_array(json_cstr, "\"a\":");
        
        update.exchange = "Binance";
        update.symbol = "BTCUSDT";
        update.is_snapshot = true;
        update.timestamp = 0; // Simplified
        
        return !update.bids.empty() || !update.asks.empty();
    }
    
    static bool parse_bybit_format(const string& json, OrderbookUpdate& update) {
        if (json.find("\"data\":") == string::npos) return false;
        
        const char* json_cstr = json.c_str();
        const char* data_start = strstr(json_cstr, "\"data\":");
        if (!data_start) return false;
        
        update.bids = extract_price_quantity_array(data_start, "\"b\":");
        update.asks = extract_price_quantity_array(data_start, "\"a\":");
        
        update.exchange = "Bybit";
        update.symbol = "BTCUSDT";
        update.is_snapshot = json.find("\"type\":\"snapshot\"") != string::npos;
        update.timestamp = 0; // Simplified
        
        return !update.bids.empty() || !update.asks.empty();
    }
    
    static bool parse_okx_format(const string& json, OrderbookUpdate& update) {
        if (json.find("\"data\":") == string::npos) return false;
        
        const char* json_cstr = json.c_str();
        const char* data_start = strstr(json_cstr, "\"data\":");
        if (!data_start) return false;
        
        update.bids = extract_price_quantity_array(data_start, "\"bids\":");
        update.asks = extract_price_quantity_array(data_start, "\"asks\":");
        
        update.exchange = "OKX";
        update.symbol = "BTCUSDT";
        update.is_snapshot = true; 
        update.timestamp = 0; 
        
        return !update.bids.empty() || !update.asks.empty();
    }
    
    static void initialize_parsers() {
        price_buffer.reserve(1000);
        quantity_buffer.reserve(1000);
    }
    
    static void cleanup_parsers() {
        price_buffer.clear();
        quantity_buffer.clear();
        field_cache.clear();
    }
};

thread_local vector<string> FastJsonParser::price_buffer;
thread_local vector<string> FastJsonParser::quantity_buffer;
thread_local unordered_map<string, string> FastJsonParser::field_cache;

#endif
