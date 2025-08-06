#ifndef FAST_MATH_HPP
#define FAST_MATH_HPP

#include <immintrin.h>
#include <cmath>
#include <vector>

using namespace std;

class FastMath {
public:
    static void calculate_arbitrage_batch(
        const vector<double>& prices_a,
        const vector<double>& prices_b,
        vector<double>& profit_results,
        size_t count
    );
    
    static double fast_exp(double x);
    
    static void calculate_percentage_batch(
        const vector<double>& numerators,
        const vector<double>& denominators, 
        vector<double>& results,
        size_t count
    );
    
    static double calculate_spread_simd(double bid, double ask);
    
    static double fast_sqrt(double x);
    
    static pair<double, double> find_min_max_simd(const vector<double>& values);
};

constexpr double FAST_PI = 3.14159265358979323846;
constexpr double FAST_E = 2.71828182845904523536;
constexpr double EPSILON = 1e-9;

#endif
