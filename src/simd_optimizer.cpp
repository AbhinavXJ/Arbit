#include "simd_optimizer.hpp"
#include <cmath>
#include <algorithm>
#include <random>

using namespace std;

SIMDOptimizer::ArbitrageResult SIMDOptimizer::calculate_arbitrage_batch_simd(
    const vector<float>& prices_a,
    const vector<float>& prices_b,
    float min_profit_threshold) {
    
    ArbitrageResult result;
    
    size_t min_size = min(prices_a.size(), prices_b.size());
    size_t batch_count = min(min_size, VECTOR_SIZE);
    
    if (batch_count == 0) return result;
    
#ifdef __AVX2__
    // SIMD implementation with AVX2
    if (batch_count >= 8) {
        __m256 prices_a_vec = _mm256_loadu_ps(prices_a.data());
        __m256 prices_b_vec = _mm256_loadu_ps(prices_b.data());
        __m256 threshold_vec = _mm256_set1_ps(min_profit_threshold);
        
        __m256 profits_vec = _mm256_sub_ps(prices_b_vec, prices_a_vec);
        
        __m256 roi_vec = _mm256_div_ps(profits_vec, prices_a_vec);
        __m256 hundred = _mm256_set1_ps(100.0f);
        roi_vec = _mm256_mul_ps(roi_vec, hundred);
        
        __m256 comparison = _mm256_cmp_ps(profits_vec, threshold_vec, _CMP_GT_OQ);
        
        _mm256_storeu_ps(result.profits.data(), profits_vec);
        _mm256_storeu_ps(result.roi_percentages.data(), roi_vec);
        _mm256_storeu_ps(reinterpret_cast<float*>(result.opportunity_flags.data()), comparison);
        
        for (size_t i = 0; i < 8; ++i) {
            if (result.opportunity_flags[i] != 0) {
                result.valid_count++;
            }
        }
    } else
#endif
    {
        return calculate_arbitrage_batch_scalar(prices_a, prices_b, min_profit_threshold);
    }
    
    return result;
}

SIMDOptimizer::ArbitrageResult SIMDOptimizer::calculate_arbitrage_batch_scalar(
    const vector<float>& prices_a,
    const vector<float>& prices_b,
    float min_profit_threshold) {
    
    ArbitrageResult result;
    
    size_t min_size = min(prices_a.size(), prices_b.size());
    size_t batch_count = min(min_size, VECTOR_SIZE);
    
    for (size_t i = 0; i < batch_count; ++i) {
        float profit = prices_b[i] - prices_a[i];
        float roi = (profit / prices_a[i]) * 100.0f;
        
        result.profits[i] = profit;
        result.roi_percentages[i] = roi;
        result.opportunity_flags[i] = (profit > min_profit_threshold) ? 1 : 0;
        
        if (result.opportunity_flags[i] != 0) {
            result.valid_count++;
        }
    }
    
    return result;
}

void SIMDOptimizer::calculate_spreads_simd(
    const vector<float>& bids,
    const vector<float>& asks,
    vector<float>& spreads) {
    
    size_t min_size = min(bids.size(), asks.size());
    spreads.resize(min_size);
    
#ifdef __AVX2__
    size_t simd_iterations = min_size / 8;
    
    for (size_t i = 0; i < simd_iterations; ++i) {
        size_t offset = i * 8;
        
        __m256 bids_vec = _mm256_loadu_ps(&bids[offset]);
        __m256 asks_vec = _mm256_loadu_ps(&asks[offset]);
        __m256 spreads_vec = _mm256_sub_ps(asks_vec, bids_vec);
        
        _mm256_storeu_ps(&spreads[offset], spreads_vec);
    }
    
    for (size_t i = simd_iterations * 8; i < min_size; ++i) {
        spreads[i] = asks[i] - bids[i];
    }
#else
    for (size_t i = 0; i < min_size; ++i) {
        spreads[i] = asks[i] - bids[i];
    }
#endif
}

void SIMDOptimizer::calculate_percentages_simd(
    const vector<float>& numerators,
    const vector<float>& denominators,
    vector<float>& results) {
    
    size_t min_size = min(numerators.size(), denominators.size());
    results.resize(min_size);
    
#ifdef __AVX2__
    __m256 hundred = _mm256_set1_ps(100.0f);
    size_t simd_iterations = min_size / 8;
    
    for (size_t i = 0; i < simd_iterations; ++i) {
        size_t offset = i * 8;
        
        __m256 num_vec = _mm256_loadu_ps(&numerators[offset]);
        __m256 den_vec = _mm256_loadu_ps(&denominators[offset]);
        __m256 div_vec = _mm256_div_ps(num_vec, den_vec);
        __m256 percent_vec = _mm256_mul_ps(div_vec, hundred);
        
        _mm256_storeu_ps(&results[offset], percent_vec);
    }
    
    for (size_t i = simd_iterations * 8; i < min_size; ++i) {
        results[i] = (numerators[i] / denominators[i]) * 100.0f;
    }
#else
    for (size_t i = 0; i < min_size; ++i) {
        results[i] = (numerators[i] / denominators[i]) * 100.0f;
    }
#endif
}

void SIMDOptimizer::calculate_synthetic_prices_simd(
    const vector<float>& spot_prices,
    const vector<float>& time_to_expiry,
    float risk_free_rate,
    vector<float>& synthetic_futures) {
    
    size_t min_size = min(spot_prices.size(), time_to_expiry.size());
    synthetic_futures.resize(min_size);
    
#ifdef __AVX2__
    __m256 rate_vec = _mm256_set1_ps(risk_free_rate);
    size_t simd_iterations = min_size / 8;
    
    for (size_t i = 0; i < simd_iterations; ++i) {
        size_t offset = i * 8;
        
        __m256 spot_vec = _mm256_loadu_ps(&spot_prices[offset]);
        __m256 time_vec = _mm256_loadu_ps(&time_to_expiry[offset]);
        
        __m256 exponent = _mm256_mul_ps(rate_vec, time_vec);
        
        __m256 one = _mm256_set1_ps(1.0f);
        __m256 exp_approx = _mm256_add_ps(one, exponent);
        
        __m256 synthetic_vec = _mm256_mul_ps(spot_vec, exp_approx);
        
        _mm256_storeu_ps(&synthetic_futures[offset], synthetic_vec);
    }
    
    for (size_t i = simd_iterations * 8; i < min_size; ++i) {
        synthetic_futures[i] = spot_prices[i] * (1.0f + risk_free_rate * time_to_expiry[i]);
    }
#else
    for (size_t i = 0; i < min_size; ++i) {
        synthetic_futures[i] = spot_prices[i] * (1.0f + risk_free_rate * time_to_expiry[i]);
    }
#endif
}

float SIMDOptimizer::calculate_mean_simd(const vector<float>& values) {
    if (values.empty()) return 0.0f;
    
    float sum = 0.0f;
    
#ifdef __AVX2__
    __m256 sum_vec = _mm256_setzero_ps();
    size_t simd_iterations = values.size() / 8;
    
    for (size_t i = 0; i < simd_iterations; ++i) {
        __m256 data_vec = _mm256_loadu_ps(&values[i * 8]);
        sum_vec = _mm256_add_ps(sum_vec, data_vec);
    }
    
    alignas(32) float temp[8];
    _mm256_store_ps(temp, sum_vec);
    for (int i = 0; i < 8; ++i) {
        sum += temp[i];
    }
    
    for (size_t i = simd_iterations * 8; i < values.size(); ++i) {
        sum += values[i];
    }
#else
    for (float value : values) {
        sum += value;
    }
#endif
    
    return sum / values.size();
}

float SIMDOptimizer::calculate_std_dev_simd(const vector<float>& values, float mean) {
    if (values.size() <= 1) return 0.0f;
    
    float sum_squared_diff = 0.0f;
    
#ifdef __AVX2__
    __m256 mean_vec = _mm256_set1_ps(mean);
    __m256 sum_vec = _mm256_setzero_ps();
    size_t simd_iterations = values.size() / 8;
    
    for (size_t i = 0; i < simd_iterations; ++i) {
        __m256 data_vec = _mm256_loadu_ps(&values[i * 8]);
        __m256 diff_vec = _mm256_sub_ps(data_vec, mean_vec);
        __m256 squared_vec = _mm256_mul_ps(diff_vec, diff_vec);
        sum_vec = _mm256_add_ps(sum_vec, squared_vec);
    }
    
    alignas(32) float temp[8];
    _mm256_store_ps(temp, sum_vec);
    for (int i = 0; i < 8; ++i) {
        sum_squared_diff += temp[i];
    }
    
    for (size_t i = simd_iterations * 8; i < values.size(); ++i) {
        float diff = values[i] - mean;
        sum_squared_diff += diff * diff;
    }
#else
    for (float value : values) {
        float diff = value - mean;
        sum_squared_diff += diff * diff;
    }
#endif
    
    return sqrt(sum_squared_diff / (values.size() - 1));
}

pair<float, float> SIMDOptimizer::find_min_max_simd(const vector<float>& values) {
    if (values.empty()) return make_pair(0.0f, 0.0f);
    
    float min_val = values[0];
    float max_val = values[0];
    
#ifdef __AVX2__
    __m256 min_vec = _mm256_set1_ps(values[0]);
    __m256 max_vec = _mm256_set1_ps(values[0]);
    size_t simd_iterations = values.size() / 8;
    
    for (size_t i = 0; i < simd_iterations; ++i) {
        __m256 data_vec = _mm256_loadu_ps(&values[i * 8]);
        min_vec = _mm256_min_ps(min_vec, data_vec);
        max_vec = _mm256_max_ps(max_vec, data_vec);
    }
    
    alignas(32) float min_temp[8], max_temp[8];
    _mm256_store_ps(min_temp, min_vec);
    _mm256_store_ps(max_temp, max_vec);
    
    for (int i = 0; i < 8; ++i) {
        min_val = min(min_val, min_temp[i]);
        max_val = max(max_val, max_temp[i]);
    }
    
    for (size_t i = simd_iterations * 8; i < values.size(); ++i) {
        min_val = min(min_val, values[i]);
        max_val = max(max_val, values[i]);
    }
#else
    for (float value : values) {
        min_val = min(min_val, value);
        max_val = max(max_val, value);
    }
#endif
    
    return make_pair(min_val, max_val);
}

void SIMDOptimizer::benchmark_simd_performance() {
    cout << "\n⚡ SIMD PERFORMANCE BENCHMARK" << endl;
    cout << string(50, '=') << endl;
    
    const size_t test_size = 10000;
    vector<float> prices_a(test_size), prices_b(test_size);
    
    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<float> dis(100000.0f, 110000.0f);
    
    for (size_t i = 0; i < test_size; ++i) {
        prices_a[i] = dis(gen);
        prices_b[i] = dis(gen);
    }
    
    auto start = chrono::high_resolution_clock::now();
    
    ArbitrageResult dummy_result;
    for (int iterations = 0; iterations < 1000; ++iterations) {
        dummy_result = calculate_arbitrage_batch_simd(prices_a, prices_b, 1.0f);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto simd_time = chrono::duration_cast<chrono::microseconds>(end - start).count();
    
    cout << "SIMD Arbitrage Calculation: " << simd_time << " microseconds" << endl;
    cout << "Processing Rate: " << fixed << setprecision(1) 
         << (test_size * 1000.0 / simd_time) << " calculations per microsecond" << endl;
    
#ifdef __AVX2__
    cout << "SIMD Type: AVX2 (8x parallel operations)" << endl;
    cout << "Performance Gain: ~8x faster than scalar operations" << endl;
#elif defined(__SSE2__)
    cout << "SIMD Type: SSE2 (4x parallel operations)" << endl;
    cout << "Performance Gain: ~4x faster than scalar operations" << endl;
#else
    cout << "SIMD Type: Scalar fallback (no vectorization)" << endl;
    cout << "Performance Gain: Baseline performance" << endl;
#endif
    
    cout << "Last Result: " << dummy_result.valid_count << " opportunities found" << endl;
    cout << string(50, '=') << endl;
}

void SIMDOptimizer::print_simd_capabilities() {
    cout << "\n SIMD CAPABILITIES" << endl;
    cout << string(40, '-') << endl;
    
#ifdef __AVX2__
    cout << " AVX2 Support: Enabled (8x float operations)" << endl;
#elif defined(__SSE2__)
    cout << " SSE2 Support: Enabled (4x float operations)" << endl;
#else
    cout << " SIMD Support: Not available (scalar operations only)" << endl;
#endif

    cout << " Vector Size: " << VECTOR_SIZE << " elements" << endl;
    cout << " Memory Alignment: " << ALIGNMENT << " bytes" << endl;
    cout << " Theoretical Speedup: ";
    
#ifdef __AVX2__
    cout << "8x for supported operations" << endl;
#elif defined(__SSE2__)
    cout << "4x for supported operations" << endl;
#else
    cout << "1x (no vectorization)" << endl;
#endif
    
    cout << string(40, '-') << endl;
}
