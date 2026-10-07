#pragma once
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace testing {
struct Test {
    const char* name;
    void (*run)();
};
inline std::vector<Test>& tests() {
    static std::vector<Test> result;
    return result;
}
struct Register {
    Register(const char* name, void (*run)()) { tests().push_back({name, run}); }
};
#define TEST(name)                                                                                 \
    void name();                                                                                   \
    Register register_##name(#name, name);                                                         \
    void name()
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition))                                                                          \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     ": " #condition);                                             \
    } while (false)
#define NEAR(a, b) CHECK(std::abs((a) - (b)) < 0.0001)
template <class F> void rejects(F function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}
inline int run_tests() {
    int failed = 0;
    for (const auto& test : tests()) {
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "FAIL " << test.name << ": " << e.what() << '\n';
        }
    }
    std::cout << tests().size() << " tests, " << failed << " failed\n";
    return failed ? 1 : 0;
}

} // namespace testing
