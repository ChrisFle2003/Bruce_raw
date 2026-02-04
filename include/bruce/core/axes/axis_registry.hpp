#pragma once
#include "bruce/types.hpp"
#include <span>
#include <vector>
#include <string>

namespace bruce::core {

struct AxisLabels { std::string negative; std::string positive; };

struct AxisRegistry {
  dim_t n{};
  std::span<const val_t> minv;
  std::span<const val_t> maxv;
  std::span<const val_t> max_abs;
  std::span<const AxisLabels> labels; // Optional human labels (core-safe: not used for logic)

  inline val_t clamp(dim_t i, std::int32_t v) const {
    auto lo = (std::int32_t)minv[i];
    auto hi = (std::int32_t)maxv[i];
    if (v < lo) return (val_t)lo;
    if (v > hi) return (val_t)hi;
    return (val_t)v;
  }
};

struct OwnedAxes {
  std::vector<val_t> minv, maxv, max_abs;
  std::vector<AxisLabels> labels;

  AxisRegistry view() const {
    AxisRegistry a;
    a.n = (dim_t)minv.size();
    a.minv = minv;
    a.maxv = maxv;
    a.max_abs = max_abs;
    a.labels = labels;
    return a;
  }
};

} // namespace bruce::core
