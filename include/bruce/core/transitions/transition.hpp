#pragma once
#include "bruce/types.hpp"

namespace bruce::core {

struct Transition {
  state_id_t from{};
  state_id_t to{};
  rule_id_t rule{};
};

}
