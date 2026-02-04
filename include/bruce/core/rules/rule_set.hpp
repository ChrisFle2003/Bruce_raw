#pragma once
#include "rule.hpp"
#include "bruce/core/axes/axis_registry.hpp"
#include "bruce/core/state/state_store.hpp"
#include <string>
#include <vector>

namespace bruce::core {

class RuleSet {
public:
  // Semantic version tag to ensure caller and rules agree on expectations.
  std::string core_version{"bruce_v6"};

  rule_id_t add(Rule r);
  const Rule* get(rule_id_t id) const;
  std::size_t size() const { return rules_.size(); }
  const std::vector<Rule>& all() const { return rules_; }

  // Returns RuleIDs that allow a transition from->to.
  std::vector<rule_id_t> allows(const State& from, const State& to, const AxisRegistry& axes) const;

private:
  std::vector<Rule> rules_;
};

}
