#pragma once
#include "bruce/types.hpp"
#include <string>
#include <vector>

namespace bruce::core {

// Minimal deterministic transition rule:
// allows transition if for all axes abs(to[i] - from[i]) <= max_abs_delta[i].
struct Rule {
  rule_id_t id{};
  std::string name;
  std::vector<val_t> max_abs_delta; // per axis thresholds
};

}
