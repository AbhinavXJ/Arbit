cmake_minimum_required(VERSION 3.10)
project(Arbit)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0")
        message(FATAL_ERROR "GCC version must be at least 7.0 for C++17 support")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra -std=c++17")

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(Arbit
    src/main.cpp
    src/book_storage.cpp
    src/binance_ws.cpp
    src/binance_futures_ws.cpp
    src/bybit_ws.cpp
    src/okx_ws.cpp
    src/synthetic_engine.cpp
    src/risk_manager.cpp
    src/performance_monitor.cpp
    src/multi_leg_arbitrage.cpp
    src/simd_optimizer.cpp
    src/eth_binance_ws.cpp
    src/eth_okx_ws.cpp
    src/eth_bybit_ws.cpp
    src/real_volatility_arbitrage.cpp
    src/real_cross_asset_arbitrage.cpp
)

target_include_directories(Arbit PRIVATE include)
target_compile_features(Arbit PRIVATE cxx_std_17)

target_link_libraries(Arbit
    OpenSSL::SSL          
    OpenSSL::Crypto         
    pthread
)
