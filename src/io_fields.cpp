#include "bruce/io/project.hpp"
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace bruce::io {

const ProjectionRule* find_projection(const Project& p, int id) {
  auto it = p.projection_index.find(id);
  if (it == p.projection_index.end()) return nullptr;
  return &p.projections[it->second];
}

const FieldDef* find_field(const Project& p, int id) {
  auto it = p.field_index.find(id);
  if (it == p.field_index.end()) return nullptr;
  return &p.fields[it->second];
}

static std::int64_t shift_right_arithmetic(std::int64_t value, int shift) {
  if (shift <= 0) return value;
  if (value >= 0) return value >> shift;
  return -(((-value) >> shift));
}

static int clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static int compute_projection(const ProjectionRule& proj, const std::vector<bruce::val_t>& vec) {
  std::int64_t sum = 0;
  for (std::size_t i = 0; i < proj.weights.size(); ++i) {
    sum += static_cast<std::int64_t>(proj.weights[i]) * static_cast<std::int64_t>(vec[i]);
  }
  std::int64_t shifted = shift_right_arithmetic(sum, proj.shift);
  std::int64_t clamped = shifted;
  if (clamped < proj.clamp_min) clamped = proj.clamp_min;
  if (clamped > proj.clamp_max) clamped = proj.clamp_max;
  return static_cast<int>(clamped);
}

static std::uint32_t bin_1d(int v, int lo, int hi, std::uint32_t grid) {
  if (hi <= lo) return 0;
  long long num = static_cast<long long>(v - lo) * static_cast<long long>(grid);
  long long den = static_cast<long long>(hi - lo + 1);
  long long b = num / den;
  if (b < 0) b = 0;
  if (b >= static_cast<long long>(grid)) b = static_cast<long long>(grid) - 1;
  return static_cast<std::uint32_t>(b);
}

static std::int8_t compute_resonance(int x, int y, int x_min, int x_max, int y_min, int y_max) {
  int abs_x_max = std::max(std::abs(x_min), std::abs(x_max));
  int abs_y_max = std::max(std::abs(y_min), std::abs(y_max));
  std::int64_t emax = static_cast<std::int64_t>(abs_x_max) + static_cast<std::int64_t>(abs_y_max);
  std::int64_t e = static_cast<std::int64_t>(std::abs(x)) + static_cast<std::int64_t>(std::abs(y));

  int magnitude = 0;
  if (emax > 0) {
    std::int64_t percent = (e * 100) / emax;
    if (percent < 20) magnitude = 0;
    else if (percent < 60) magnitude = 1;
    else magnitude = 2;
  }

  int sign = 0;
  int sum = x + y;
  if (sum > 0) sign = 1;
  else if (sum < 0) sign = -1;
  return static_cast<std::int8_t>(sign * magnitude);
}

FieldCache build_field_cache(const Project& p, const FieldDef& field) {
  const ProjectionRule* x_proj = find_projection(p, field.x_proj);
  const ProjectionRule* y_proj = find_projection(p, field.y_proj);
  if (!x_proj || !y_proj) {
    throw std::runtime_error("field build: missing projection id");
  }
  if (field.grid != 5) {
    throw std::runtime_error("field build: grid must be 5");
  }

  const std::uint32_t g = field.grid;
  FieldCache cache;
  cache.field_id = field.id;
  cache.x_proj = field.x_proj;
  cache.y_proj = field.y_proj;
  cache.grid = g;
  cache.cells.assign(g, std::vector<std::vector<bruce::state_id_t>>(g));
  cache.resonance.assign(g, std::vector<std::int8_t>(g, 0));

  std::vector<std::vector<int>> res_sum(g, std::vector<int>(g, 0));
  std::vector<std::vector<int>> res_count(g, std::vector<int>(g, 0));

  auto ids = p.state_store.ids();
  for (auto sid : ids) {
    const auto* s = p.state_store.get(sid);
    if (!s) continue;
    if (s->core_id != field.selector.core_id) continue;

    int x = compute_projection(*x_proj, s->vec);
    int y = compute_projection(*y_proj, s->vec);
    std::uint32_t bx = bin_1d(x, x_proj->clamp_min, x_proj->clamp_max, g);
    std::uint32_t by = bin_1d(y, y_proj->clamp_min, y_proj->clamp_max, g);
    cache.cells[by][bx].push_back(s->id);

    std::int8_t r = compute_resonance(x, y, x_proj->clamp_min, x_proj->clamp_max, y_proj->clamp_min, y_proj->clamp_max);
    res_sum[by][bx] += static_cast<int>(r);
    res_count[by][bx] += 1;
  }

  for (std::uint32_t y = 0; y < g; ++y) {
    for (std::uint32_t x = 0; x < g; ++x) {
      auto& lst = cache.cells[y][x];
      std::sort(lst.begin(), lst.end());
      if (res_count[y][x] == 0) {
        cache.resonance[y][x] = 0;
      } else {
        int avg = res_sum[y][x] / res_count[y][x];
        cache.resonance[y][x] = static_cast<std::int8_t>(avg);
      }
    }
  }

  return cache;
}

std::vector<FieldCache> build_all_field_caches(const Project& p) {
  std::vector<FieldCache> out;
  out.reserve(p.fields.size());
  for (const auto& field : p.fields) {
    out.push_back(build_field_cache(p, field));
  }
  return out;
}

} // namespace bruce::io
