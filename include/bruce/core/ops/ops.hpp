#pragma once
#include "bruce/core/axes/axis_registry.hpp"
#include "bruce/core/state/state.hpp"
#include <cassert>
#include <cstdint>
#include <cstdlib>

namespace bruce::core::ops {

inline void mix_into(const std::vector<val_t>& a, const std::vector<val_t>& b, const AxisRegistry& axes, std::vector<val_t>& out) {
  assert(a.size() == axes.n && b.size() == axes.n && out.size() == axes.n);
  for (dim_t i = 0; i < axes.n; ++i) {
    std::int32_t s = (std::int32_t)a[i] + (std::int32_t)b[i];
    std::int32_t mid = s / 2;          // round-to-zero
    out[i] = axes.clamp(i, mid);       // clamp
  }
}

inline std::int64_t l1_distance(const std::vector<val_t>& a, const std::vector<val_t>& b) {
  assert(a.size() == b.size());
  std::int64_t total = 0;
  for (std::size_t i = 0; i < a.size(); ++i) total += std::llabs((long long)a[i] - (long long)b[i]);
  return total;
}

inline double collapse_to_neutral(const std::vector<val_t>& v, const AxisRegistry& axes) {
  assert(v.size() == axes.n);
  std::int64_t total = 0, max_total = 0;
  for (dim_t i = 0; i < axes.n; ++i) {
    total += std::llabs((long long)v[i]);
    max_total += (std::int64_t)axes.max_abs[i];
  }
  if (max_total == 0) return 1.0;
  double frac = (double)total / (double)max_total;
  double score = 1.0 - frac;
  if (score < 0.0) score = 0.0;
  if (score > 1.0) score = 1.0;
  return score;
}

} // namespace bruce::core::ops
