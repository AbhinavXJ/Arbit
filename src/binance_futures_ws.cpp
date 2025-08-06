#include "binance_futures_ws.hpp"
#include "book_storage.hpp"
#include "json.hpp"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <iostream>
#include <string>
#include <chrono>


using namespace std;
using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

void start_binance_futures_ws() {
    client c;
    string uri = "wss://fstream.binance.com/ws/btcusdt@depth";
    
    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.set_error_channels(websocketpp::log::elevel::none);
        
        c.init_asio();
        c.set_tls_init_handler([](websocketpp::connection_hdl) {
            return make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        });

        c.set_open_handler([](websocketpp::connection_hdl) {
            cout << "[BINANCE FUTURES]  Connected" << endl;
        });

        c.set_message_handler([&](websocketpp::connection_hdl, client::message_ptr msg) {
            try {
                json data = json::parse(msg->get_payload());
                
                if (data.contains("b") && data.contains("a")) {
                    lock_guard<mutex> lock(book_mutex);
                    binance_futures_timestamp.last_update = chrono::steady_clock::now();
                    
                    for (auto& bid : data["b"]) {
                        if (bid.size() >= 2) {
                            double price = stod(bid[0].get<string>());
                            string qty = bid[1].get<string>();
                            
                            if (stod(qty) == 0.0) {
                                futures_bids.erase(price);
                            } else {
                                futures_bids[price] = qty;
                            }
                        }
                    } 
                    
                    for (auto& ask : data["a"]) {
                        if (ask.size() >= 2) {
                            double price = stod(ask[0].get<string>());
                            string qty = ask[1].get<string>();
                            
                            if (stod(qty) == 0.0) {
                                futures_asks.erase(price);
                            } else {
                                futures_asks[price] = qty;
                            }
                        }
                    } 
                }
            } catch (const exception& e) {
            }
        });

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri, ec);
        if (ec) {
            cout << "[BINANCE FUTURES ERROR] Connect error: " << ec.message() << endl;
            return;
        }
        
        c.connect(con);
        c.run();

    } catch (const exception& e) {
        cout << "[BINANCE FUTURES ERROR] Exception: " << e.what() << endl;
    }
}
