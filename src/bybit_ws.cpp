#include "bybit_ws.hpp"
#include "book_storage.hpp"
#include "json.hpp"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

using namespace std;
using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

void start_bybit_spot_ws() {
    client c;
    string uri = "wss://stream.bybit.com/v5/public/spot";
    
    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.set_error_channels(websocketpp::log::elevel::none);
        
        c.init_asio();
        c.set_tls_init_handler([](websocketpp::connection_hdl) {
            return make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        });

        c.set_open_handler([&](websocketpp::connection_hdl hdl) {
            cout << "[BYBIT SPOT]  Connected" << endl;
            
            json subscribe_msg = {
                {"op", "subscribe"},
                {"args", {"orderbook.50.BTCUSDT"}}
            };
            
            websocketpp::lib::error_code ec;
            c.send(hdl, subscribe_msg.dump(), websocketpp::frame::opcode::text, ec);
            if (ec) {
                cout << "[BYBIT SPOT] Send error: " << ec.message() << endl;
            }
        });

        c.set_fail_handler([](websocketpp::connection_hdl) {
            cout << "[BYBIT SPOT]  Connection failed!" << endl;
        });

        c.set_close_handler([](websocketpp::connection_hdl) {
            cout << "[BYBIT SPOT]  Connection closed!" << endl;
        });

        c.set_message_handler([&](websocketpp::connection_hdl, client::message_ptr msg) {
            try {
                string payload = msg->get_payload();
                json data = json::parse(payload);
                
                if (data.contains("success") || data.contains("ret_msg")) {
                    return;
                }
                
                if (data.contains("data") && data["data"].contains("b") && data["data"].contains("a")) {
                    lock_guard<mutex> lock(book_mutex);
                    bybit_spot_timestamp.last_update = chrono::steady_clock::now();
                    
                    if (data.contains("type") && data["type"] == "snapshot") {
                        bybit_spot_bids.clear();
                        bybit_spot_asks.clear();
                    }
                    
                    for (auto& bid : data["data"]["b"]) {
                        if (bid.size() >= 2) {
                            string price_str = bid[0].get<string>();
                            string qty = bid[1].get<string>();
                            double price = stod(price_str);
                            
                            if (stod(qty) == 0.0) {
                                bybit_spot_bids.erase(price);
                            } else {
                                bybit_spot_bids[price] = qty;
                            }
                        }
                    }
                    
                    for (auto& ask : data["data"]["a"]) {
                        if (ask.size() >= 2) {
                            string price_str = ask[0].get<string>();
                            string qty = ask[1].get<string>();
                            double price = stod(price_str);
                            
                            if (stod(qty) == 0.0) {
                                bybit_spot_asks.erase(price);
                            } else {
                                bybit_spot_asks[price] = qty;
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
            cout << "[BYBIT SPOT ERROR] Connect error: " << ec.message() << endl;
            return;
        }

        cout << "[BYBIT SPOT]  Connecting to Bybit Spot WebSocket..." << endl;
        c.connect(con);
        c.run();

    } catch (const exception& e) {
        cout << "[BYBIT SPOT ERROR] Exception: " << e.what() << endl;
    }
}

void start_bybit_futures_ws() {
    client c;
    string uri = "wss://stream.bybit.com/v5/public/linear";
    
    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.set_error_channels(websocketpp::log::elevel::none);
        
        c.init_asio();
        c.set_tls_init_handler([](websocketpp::connection_hdl) {
            return make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        });

        c.set_open_handler([&](websocketpp::connection_hdl hdl) {
            cout << "[BYBIT FUTURES]  Connected" << endl;
            
            json subscribe_msg = {
                {"op", "subscribe"},
                {"args", {"orderbook.50.BTCUSDT"}}
            };
            
            websocketpp::lib::error_code ec;
            c.send(hdl, subscribe_msg.dump(), websocketpp::frame::opcode::text, ec);
            if (ec) {
                cout << "[BYBIT FUTURES] Send error: " << ec.message() << endl;
            }
        });

        c.set_fail_handler([](websocketpp::connection_hdl) {
            cout << "[BYBIT FUTURES]  Connection failed!" << endl;
        });

        c.set_close_handler([](websocketpp::connection_hdl) {
            cout << "[BYBIT FUTURES]  Connection closed!" << endl;
        });

        c.set_message_handler([&](websocketpp::connection_hdl, client::message_ptr msg) {
            try {
                string payload = msg->get_payload();
                json data = json::parse(payload);
                
                if (data.contains("success") || data.contains("ret_msg")) {
                    return;
                }
                
                if (data.contains("data") && data["data"].contains("b") && data["data"].contains("a")) {
                    lock_guard<mutex> lock(book_mutex);
                    bybit_futures_timestamp.last_update = chrono::steady_clock::now();
                    
                    if (data.contains("type") && data["type"] == "snapshot") {
                        bybit_futures_bids.clear();
                        bybit_futures_asks.clear();
                    }
                    
                    for (auto& bid : data["data"]["b"]) {
                        if (bid.size() >= 2) {
                            string price_str = bid[0].get<string>();
                            string qty = bid[1].get<string>();
                            double price = stod(price_str);
                            
                            if (stod(qty) == 0.0) {
                                bybit_futures_bids.erase(price);
                            } else {
                                bybit_futures_bids[price] = qty;
                            }
                        }
                    }
                    
                    for (auto& ask : data["data"]["a"]) {
                        if (ask.size() >= 2) {
                            string price_str = ask[0].get<string>();
                            string qty = ask[1].get<string>();
                            double price = stod(price_str);
                            
                            if (stod(qty) == 0.0) {
                                bybit_futures_asks.erase(price);
                            } else {
                                bybit_futures_asks[price] = qty;
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
            cout << "[BYBIT FUTURES ERROR] Connect error: " << ec.message() << endl;
            return;
        }

        cout << "[BYBIT FUTURES]  Connecting to Bybit Futures WebSocket..." << endl;
        c.connect(con);
        c.run();

    } catch (const exception& e) {
        cout << "[BYBIT FUTURES ERROR] Exception: " << e.what() << endl;
    }
}
