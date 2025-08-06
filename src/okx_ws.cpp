#include "okx_ws.hpp"
#include "book_storage.hpp"
#include "json.hpp"
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace std;
using json = nlohmann::json;
typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

void start_okx_spot_ws() {
    client c;
    string uri = "wss://ws.okx.com:8443/ws/v5/public";
    
    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.set_error_channels(websocketpp::log::elevel::none);
        c.init_asio();
        
        c.set_tls_init_handler([](websocketpp::connection_hdl) {
            return make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        });
        
        c.set_open_handler([&](websocketpp::connection_hdl hdl) {
            cout << "[OKX SPOT]  Connected" << endl;
            json subscribe_msg = {
                {"op", "subscribe"},
                {"args", {{{"channel", "books"}, {"instId", "BTC-USDT"}}}}
            };
            websocketpp::lib::error_code ec;
            c.send(hdl, subscribe_msg.dump(), websocketpp::frame::opcode::text, ec);
            if (ec) {
                cout << "[OKX SPOT] Send error: " << ec.message() << endl;
            }
        });
        
        c.set_fail_handler([](websocketpp::connection_hdl) {
            cout << "[OKX SPOT]  Connection failed!" << endl;
        });
        
        c.set_close_handler([](websocketpp::connection_hdl) {
            cout << "[OKX SPOT]  Connection closed!" << endl;
        });
        
        c.set_message_handler([&](websocketpp::connection_hdl, client::message_ptr msg) {
            try {
                json data = json::parse(msg->get_payload());
                
                if (data.contains("event")) {
                    cout << "[OKX SPOT DEBUG] Event: " << data["event"] << endl;
                    return;
                }
                
                if (data.contains("data") && !data["data"].empty()) {
                    for (auto& book_data : data["data"]) {
                        if (book_data.contains("bids") && book_data.contains("asks")) {
                            lock_guard<mutex> lock(book_mutex);
                            okx_spot_timestamp.last_update = chrono::steady_clock::now();
                            
                            for (auto& bid : book_data["bids"]) {
                                if (bid.size() >= 2) {
                                    double price = stod(bid[0].get<string>());
                                    string qty = bid[1].get<string>();
                                    if (stod(qty) == 0.0) {
                                        okx_spot_bids.erase(price);
                                    } else {
                                        okx_spot_bids[price] = qty;
                                    }
                                }
                            }
                            
                            for (auto& ask : book_data["asks"]) {
                                if (ask.size() >= 2) {
                                    double price = stod(ask[0].get<string>());
                                    string qty = ask[1].get<string>();
                                    if (stod(qty) == 0.0) {
                                        okx_spot_asks.erase(price);
                                    } else {
                                        okx_spot_asks[price] = qty;
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (const exception& e) {
                cout << "[OKX SPOT ERROR] Message parsing error: " << e.what() << endl;
            }
        });

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri, ec);
        if (ec) {
            cout << "[OKX SPOT ERROR] Connect error: " << ec.message() << endl;
            return;
        }
        
        cout << "[OKX SPOT]  Connecting to OKX Spot WebSocket..." << endl;
        c.connect(con);
        c.run();
        
    } catch (const exception& e) {
        cout << "[OKX SPOT ERROR] Exception: " << e.what() << endl;
    }
}

void start_okx_futures_ws() {
    client c;
    string uri = "wss://ws.okx.com:8443/ws/v5/public";
    
    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.set_error_channels(websocketpp::log::elevel::none);
        c.init_asio();
        
        c.set_tls_init_handler([](websocketpp::connection_hdl) {
            return make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
        });
        
        c.set_open_handler([&](websocketpp::connection_hdl hdl) {
            cout << "[OKX FUTURES]  Connected" << endl;
            json subscribe_msg = {
                {"op", "subscribe"},
                {"args", {{{"channel", "books"}, {"instId", "BTC-USDT-SWAP"}}}}
            };
            websocketpp::lib::error_code ec;
            c.send(hdl, subscribe_msg.dump(), websocketpp::frame::opcode::text, ec);
            if (ec) {
                cout << "[OKX FUTURES] Send error: " << ec.message() << endl;
            }
        });
        
        c.set_fail_handler([](websocketpp::connection_hdl) {
            cout << "[OKX FUTURES]  Connection failed!" << endl;
        });
        
        c.set_close_handler([](websocketpp::connection_hdl) {
            cout << "[OKX FUTURES]  Connection closed!" << endl;
        });
        
        c.set_message_handler([&](websocketpp::connection_hdl, client::message_ptr msg) {
            try {
                json data = json::parse(msg->get_payload());
                
                if (data.contains("event")) {
                    cout << "[OKX FUTURES DEBUG] Event: " << data["event"] << endl;
                    return;
                }
                
                if (data.contains("data") && !data["data"].empty()) {
                    for (auto& book_data : data["data"]) {
                        if (book_data.contains("bids") && book_data.contains("asks")) {
                            lock_guard<mutex> lock(book_mutex);
                            okx_futures_timestamp.last_update = chrono::steady_clock::now();
                            
                            for (auto& bid : book_data["bids"]) {
                                if (bid.size() >= 2) {
                                    double price = stod(bid[0].get<string>());
                                    string qty = bid[1].get<string>();
                                    if (stod(qty) == 0.0) {
                                        okx_futures_bids.erase(price);
                                    } else {
                                        okx_futures_bids[price] = qty;
                                    }
                                }
                            }
                            
                            for (auto& ask : book_data["asks"]) {
                                if (ask.size() >= 2) {
                                    double price = stod(ask[0].get<string>());
                                    string qty = ask[1].get<string>();
                                    if (stod(qty) == 0.0) {
                                        okx_futures_asks.erase(price);
                                    } else {
                                        okx_futures_asks[price] = qty;
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (const exception& e) {
                cout << "[OKX FUTURES ERROR] Message parsing error: " << e.what() << endl;
            }
        });

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection(uri, ec);
        if (ec) {
            cout << "[OKX FUTURES ERROR] Connect error: " << ec.message() << endl;
            return;
        }
        
        cout << "[OKX FUTURES]  Connecting to OKX Futures WebSocket..." << endl;
        c.connect(con);
        c.run();
        
    } catch (const exception& e) {
        cout << "[OKX FUTURES ERROR] Exception: " << e.what() << endl;
    }
}
