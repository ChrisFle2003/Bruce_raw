#include "bruce/core/rules/rule_set.hpp"
#include <cstdlib>

namespace bruce::core {

rule_id_t RuleSet::add(Rule r) {
  if (r.id == 0) r.id = (rule_id_t)(rules_.size() + 1);
  rules_.push_back(std::move(r));
  return rules_.back().id;
}

const Rule* RuleSet::get(rule_id_t id) const {
  for (const auto& r : rules_) if (r.id == id) return &r;
  return nullptr;
}

std::vector<rule_id_t> RuleSet::allows(const State& from, const State& to, const AxisRegistry& axes) const {
  std::vector<rule_id_t> ok;
  if (from.vec.size() != axes.n || to.vec.size() != axes.n) return ok;
  if (from.core_id != to.core_id) return ok; // strict by default; relax in Observer if desired

  for (const auto& r : rules_) {
    if (r.max_abs_delta.size() != axes.n) continue;
    bool pass = true;
    for (dim_t i = 0; i < axes.n; ++i) {
      int d = (int)to.vec[i] - (int)from.vec[i];
      if (d < 0) d = -d;
      if (d > (int)r.max_abs_delta[i]) { pass = false; break; }
    }
    if (pass) ok.push_back(r.id);
  }
  return ok;
}

} // namespace bruce::core
