#pragma once
#include "bruce/core/axes/axis_registry.hpp"
#include "bruce/core/ops/ops.hpp"
#include "bruce/core/rules/rule_set.hpp"
#include "bruce/core/state/state.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace bruce::core::v6 {

// Re-export core types for convenience.
using ::bruce::core::AxisRegistry;
using ::bruce::core::RuleSet;
using ::bruce::core::State;

// Decisions that a guard can enforce.
enum class Decision {
  REJECT,
  THROTTLE,
  ACCEPT,
  ISOLATE
};

// Predicate kinds for guards.
enum class GuardPredicateKind {
  AllowedAny,
  DistanceAtLeast,
  AxisRange
};

struct GuardPredicate {
  GuardPredicateKind kind{GuardPredicateKind::AllowedAny};
  std::int64_t distance{0};             // Used for DistanceAtLeast
  std::vector<val_t> min;               // Used for AxisRange
  std::vector<val_t> max;               // Used for AxisRange
};

using guard_id_t = std::uint64_t;

struct Guard {
  guard_id_t id{};
  std::string name;
  Decision decision{Decision::REJECT};
  GuardPredicate predicate{};
};

struct DecisionResult {
  Decision decision{Decision::REJECT};
  std::vector<rule_id_t> allowed_rules;
  std::int64_t distance{0};
  bool illegal{false};
  bool version_mismatch{false};
};

class DecisionEngine {
public:
  DecisionEngine(const AxisRegistry& axes,
                 const RuleSet& rules,
                 const State& origin,
                 std::vector<Guard> guards,
                 std::string expected_core_version = "bruce_v6")
    : axes_(axes),
      rules_(rules),
      origin_(origin),
      guards_(std::move(guards)),
      expected_core_version_(std::move(expected_core_version)) {}

  DecisionResult decision(const State& candidate) const {
    DecisionResult res{};

    // Version consistency checks
    if ((!rules_.core_version.empty() && rules_.core_version != expected_core_version_) ||
        (!candidate.core_version.empty() && candidate.core_version != expected_core_version_) ||
        (!origin_.core_version.empty() && origin_.core_version != expected_core_version_)) {
      res.version_mismatch = true;
      res.decision = Decision::REJECT;
      return res;
    }

    // Basic structural validation
    if (candidate.vec.size() != axes_.n || origin_.vec.size() != axes_.n || candidate.core_id != origin_.core_id) {
      res.illegal = true;
      res.decision = Decision::REJECT;
      return res;
    }

    // Bounds check: any axis outside declared min/max => illegal
    for (dim_t i = 0; i < axes_.n; ++i) {
      auto v = candidate.vec[i];
      if (v < axes_.minv[i] || v > axes_.maxv[i]) {
        res.illegal = true;
        res.decision = Decision::REJECT;
        return res;
      }
    }

    res.distance = bruce::core::ops::l1_distance(origin_.vec, candidate.vec);
    res.allowed_rules = rules_.allows(origin_, candidate, axes_);

    // Guard evaluation in order; first passing guard wins.
    for (const auto& g : guards_) {
      if (predicate_passes(g.predicate, candidate)) {
        res.decision = g.decision;
        return res;
      }
    }

    // Fallback: if no guard triggered, reject.
    res.decision = Decision::REJECT;
    return res;
  }

private:
  bool predicate_passes(const GuardPredicate& pred, const State& candidate) const {
    switch (pred.kind) {
      case GuardPredicateKind::AllowedAny:
        return true;
      case GuardPredicateKind::DistanceAtLeast:
        return pred.distance <= 0 ? true : (bruce::core::ops::l1_distance(origin_.vec, candidate.vec) >= pred.distance);
      case GuardPredicateKind::AxisRange: {
        if (!pred.min.empty() && pred.min.size() != candidate.vec.size()) return false;
        if (!pred.max.empty() && pred.max.size() != candidate.vec.size()) return false;
        for (std::size_t i = 0; i < candidate.vec.size(); ++i) {
          if (!pred.min.empty() && candidate.vec[i] < pred.min[i]) return false;
          if (!pred.max.empty() && candidate.vec[i] > pred.max[i]) return false;
        }
        return true;
      }
    }
    return false;
  }

  AxisRegistry axes_;
  RuleSet rules_;
  State origin_;
  std::vector<Guard> guards_;
  std::string expected_core_version_;
};

} // namespace bruce::core::v6
