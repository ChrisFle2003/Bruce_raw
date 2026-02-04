#include "bruce/core/v6/core.hpp"
#include <cassert>

using bruce::core::v6::Decision;
using bruce::core::v6::DecisionEngine;
using bruce::core::v6::DecisionResult;
using bruce::core::v6::Guard;
using bruce::core::v6::GuardPredicate;
using bruce::core::v6::GuardPredicateKind;
using bruce::core::v6::RuleSet;
using bruce::core::v6::State;

static bruce::core::AxisRegistry make_axes() {
  bruce::core::OwnedAxes owned;
  owned.minv = { -5, -5 };
  owned.maxv = { 5, 5 };
  owned.max_abs = { 5, 5 };
  owned.labels = { {"negA", "posA"}, {"negB", "posB"} };
  return owned.view();
}

static RuleSet make_rules() {
  RuleSet rules;
  bruce::core::Rule rule;
  rule.id = 1;
  rule.name = "tight";
  rule.max_abs_delta = { 2, 2 };
  rules.add(rule);
  return rules;
}

static Guard make_distance_guard(bruce::core::v6::guard_id_t id,
                                 bruce::core::v6::Decision decision,
                                 std::int64_t min_distance) {
  Guard guard;
  guard.id = id;
  guard.name = "distance_guard";
  guard.decision = decision;
  guard.predicate.kind = GuardPredicateKind::DistanceAtLeast;
  guard.predicate.distance = min_distance;
  return guard;
}

static Guard make_allowed_guard(bruce::core::v6::guard_id_t id,
                                bruce::core::v6::Decision decision) {
  Guard guard;
  guard.id = id;
  guard.name = "allowed_guard";
  guard.decision = decision;
  guard.predicate.kind = GuardPredicateKind::AllowedAny;
  return guard;
}

static Guard make_axis_guard(bruce::core::v6::guard_id_t id,
                             bruce::core::v6::Decision decision,
                             const std::vector<bruce::val_t>& minv,
                             const std::vector<bruce::val_t>& maxv) {
  Guard guard;
  guard.id = id;
  guard.name = "axis_guard";
  guard.decision = decision;
  guard.predicate.kind = GuardPredicateKind::AxisRange;
  guard.predicate.min = minv;
  guard.predicate.max = maxv;
  return guard;
}

int main() {
  auto axes = make_axes();
  RuleSet rules = make_rules();

  State origin;
  origin.core_id = 7;
  origin.vec = { 0, 0 };

  State candidate;
  candidate.core_id = 7;
  candidate.vec = { 1, 1 };

  std::vector<Guard> guards = {
    make_distance_guard(1, Decision::THROTTLE, 3),
    make_allowed_guard(2, Decision::ACCEPT)
  };

  DecisionEngine engine(axes, rules, origin, guards);
  DecisionResult first = engine.decision(candidate);
  DecisionResult second = engine.decision(candidate);

  assert(first.decision == Decision::ACCEPT);
  assert(first.decision == second.decision);
  assert(first.allowed_rules == second.allowed_rules);
  assert(first.distance == second.distance);

  State illegal = candidate;
  illegal.vec = { 10, 0 };
  DecisionResult illegal_result = engine.decision(illegal);
  assert(illegal_result.decision == Decision::REJECT);
  assert(illegal_result.illegal);

  std::vector<Guard> guard_order_a = {
    make_axis_guard(10, Decision::ISOLATE, { -5, -5 }, { 5, 5 }),
    make_allowed_guard(11, Decision::ACCEPT)
  };
  std::vector<Guard> guard_order_b = {
    make_allowed_guard(11, Decision::ACCEPT),
    make_axis_guard(10, Decision::ISOLATE, { -5, -5 }, { 5, 5 })
  };

  DecisionEngine engine_a(axes, rules, origin, guard_order_a);
  DecisionEngine engine_b(axes, rules, origin, guard_order_b);
  DecisionResult decision_a = engine_a.decision(candidate);
  DecisionResult decision_b = engine_b.decision(candidate);
  assert(decision_a.decision == Decision::ISOLATE);
  assert(decision_b.decision == Decision::ACCEPT);

  State other = candidate;
  other.vec = { 2, 0 };
  DecisionResult before = engine.decision(other);
  DecisionResult after = engine.decision(other);
  assert(before.decision == after.decision);
  assert(before.distance == after.distance);

  RuleSet bad_rules = rules;
  bad_rules.core_version = "bruce_v5";
  DecisionEngine engine_bad_rules(axes, bad_rules, origin, guards);
  DecisionResult bad_rules_result = engine_bad_rules.decision(candidate);
  assert(bad_rules_result.decision == Decision::REJECT);
  assert(bad_rules_result.version_mismatch);

  State bad_state = candidate;
  bad_state.core_version = "bruce_v5";
  DecisionResult bad_state_result = engine.decision(bad_state);
  assert(bad_state_result.decision == Decision::REJECT);
  assert(bad_state_result.version_mismatch);

  return 0;
}
