#ifndef PERFORMANCE_MONITOR_HPP
#define PERFORMANCE_MONITOR_HPP

#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <iostream>
#include <iomanip>

using namespace std;
using namespace chrono;

struct PerformanceMetrics {
    atomic<uint64_t> total_messages{0};
    atomic<uint64_t> messages_per_second{0};
    atomic<uint64_t> avg_processing_time_ns{0};
    atomic<uint64_t> max_processing_time_ns{0};
    atomic<uint64_t> min_processing_time_ns{UINT64_MAX};
};

class PerformanceMonitor;

class PerformanceTimer {
private:
    high_resolution_clock::time_point start_time;
    string operation_name;
    
public:
    PerformanceTimer(const string& name) : operation_name(name) {
        start_time = high_resolution_clock::now();
    }
    
    ~PerformanceTimer();
};

class PerformanceMonitor {
private:
    static unordered_map<string, PerformanceMetrics> metrics;
    static mutex metrics_mutex;
    static atomic<bool> monitoring_enabled;
    
public:
    static void start_monitoring() {
        monitoring_enabled = true;
        cout << "[PERFORMANCE] Monitoring started" << endl;
    }
    
    static void stop_monitoring() {
        monitoring_enabled = false;
        cout << "[PERFORMANCE] Monitoring stopped" << endl;
    }
    
    static void record_operation(const string& operation, uint64_t duration_ns) {
        if (!monitoring_enabled) return;
        
        lock_guard<mutex> lock(metrics_mutex);
        auto& metric = metrics[operation];
        metric.total_messages++;
        metric.avg_processing_time_ns = duration_ns;
        metric.max_processing_time_ns = max(metric.max_processing_time_ns.load(), duration_ns);
        metric.min_processing_time_ns = min(metric.min_processing_time_ns.load(), duration_ns);
    }
    
    static void print_performance_report() {
        if (!monitoring_enabled) return;
        
        lock_guard<mutex> lock(metrics_mutex);
        cout << "\nPERFORMANCE REPORT" << endl;
        cout << string(60, '=') << endl;
        cout << setw(20) << "Operation" << setw(15) << "Count" 
             << setw(15) << "Avg (us)" << setw(15) << "Max (us)" << endl;
        cout << string(60, '-') << endl;
        
        for (const auto& pair : metrics) {
            const auto& metric = pair.second;
            cout << setw(20) << pair.first 
                 << setw(15) << metric.total_messages.load()
                 << setw(15) << (metric.avg_processing_time_ns.load() / 1000)
                 << setw(15) << (metric.max_processing_time_ns.load() / 1000) << endl;
        }
        cout << string(60, '=') << endl;
    }
};

inline PerformanceTimer::~PerformanceTimer() {
    auto end_time = high_resolution_clock::now();
    auto duration_ns = duration_cast<nanoseconds>(end_time - start_time).count();
    PerformanceMonitor::record_operation(operation_name, duration_ns);
}

#define PERF_TIMER(name) PerformanceTimer _timer(name)

#endif
