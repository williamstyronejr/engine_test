#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace engine {
struct StepBatch {
    int steps{};
    float alpha{};
    double dropped{};
};
class FixedClock {
  public:
    static constexpr double step = 1.0 / 60.0;
    static constexpr int max_steps = 8;
    StepBatch advance(double elapsed) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            throw std::invalid_argument("Invalid elapsed time");
        const double accepted = std::min(elapsed, 0.25);
        accumulator_ += accepted;
        const int available = static_cast<int>(std::floor((accumulator_ + 1e-12) / step));
        const int steps = std::min(available, max_steps);
        accumulator_ = std::max(0.0, accumulator_ - static_cast<double>(available) * step);
        return {steps,
                std::min(static_cast<float>(accumulator_ / step), std::nextafter(1.0F, 0.0F)),
                elapsed - accepted + static_cast<double>(available - steps) * step};
    }

  private:
    double accumulator_{};
};
} // namespace engine
