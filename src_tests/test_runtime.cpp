// Full program runtime performance test.
// This now measures the wall-clock time of launching the compiled executable
// (process start, config read, dataset load, simulation, output writing, teardown)
// similar to invoking:  ./herss.exe ./global.txt

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <filesystem>
#include <cstdlib>

TEST(PerformanceTestFullProgram, Utahps_FullProgramAverageRuntimeUnder350ms) {
    using clock = std::chrono::steady_clock;

    const std::string kExePath = "./herss.exe"; 
    const std::string kGlobalPath = "../src_tests/utahps_test/global.txt"; 
    const int kNRuns = 10; 
    const double kMaxAvgSeconds = 0.75; 

    if (!std::filesystem::exists(kExePath)) {
        GTEST_SKIP() << "Executable '" << kExePath << "' not found. Build herss.exe before running this test.";
    }
    if (!std::filesystem::exists(kGlobalPath)) {
        GTEST_SKIP() << "Global config file '" << kGlobalPath << "' not found.";
    }

    // Ensure output directory referenced by the global file exists (./output/ relative to CWD=src)
    // to avoid the run failing on first attempt. (If it already exists, no effect.)
    std::filesystem::create_directories("output");

    std::vector<double> durations;    
    durations.reserve(kNRuns);

    for (int i = 0; i < kNRuns; ++i) {
        auto start = clock::now();
        // Launch full program (no output redirection so we capture "normal" behavior)
        std::string cmd = kExePath + " " + kGlobalPath;
        int ret = std::system(cmd.c_str());
        auto end = clock::now();

        std::chrono::duration<double> dt = end - start;
        double secs = dt.count();
        durations.push_back(secs);
        std::cout << "[PerformanceTestFullProgram] Run " << (i+1) << "/" << kNRuns
                  << " = " << secs << " s (exit=" << ret << ")\n";

        ASSERT_EQ(ret, 0) << "Full program exited with non-zero status on run " << (i+1);
    }

    double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
    double avg = sum / durations.size();
    auto [min_it, max_it] = std::minmax_element(durations.begin(), durations.end());
    double minv = *min_it;
    double maxv = *max_it;
    double sq_sum = std::inner_product(durations.begin(), durations.end(), durations.begin(), 0.0);
    double stdev = std::sqrt(std::max(0.0, (sq_sum / durations.size()) - (avg * avg)));

    std::vector<double> sorted = durations;
    std::sort(sorted.begin(), sorted.end());
    double median = (kNRuns % 2) ? sorted[kNRuns/2] : 0.5 * (sorted[kNRuns/2 -1] + sorted[kNRuns/2]);

    std::cout << "[PerformanceTestFullProgram] Average runtime over " << kNRuns << " runs = "
              << avg << " s (target < " << kMaxAvgSeconds << " s)\n";
    std::cout << "[PerformanceTestFullProgram] Min=" << minv << " s  Max=" << maxv
              << " s  Median=" << median << " s  StdDev=" << stdev << " s\n";

    EXPECT_LT(avg, kMaxAvgSeconds) << "Average full-program runtime exceeded target. Avg=" << avg
                                   << " s, target < " << kMaxAvgSeconds << " s";
}
