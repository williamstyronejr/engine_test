#include "../examples/feature_lab/stress_benchmark.hpp"
#include <iostream>
int main(int argc, char** argv) {
    try {
        auto options = feature_lab::parse_stress_options(argc, argv);
        if (options.help) {
            std::cout << feature_lab::stress_help();
            return 0;
        }
        options.cpu_only = true;
        return feature_lab::run_cpu_stress(options);
    } catch (const std::exception& error) {
        std::cerr << "[stress] " << error.what() << '\n';
        return 1;
    }
}
