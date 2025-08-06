#include "performance_monitor.hpp"

using namespace std;

unordered_map<string, PerformanceMetrics> PerformanceMonitor::metrics;
mutex PerformanceMonitor::metrics_mutex;
atomic<bool> PerformanceMonitor::monitoring_enabled{false};
