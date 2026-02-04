#pragma once
#include "bruce/types.hpp"
#include <string>
#include <vector>

namespace bruce::core {

// Immutable state: values never change after creation.
struct State {
  state_id_t id{};
  core_id_t core_id{};               // Optional grouping, can be 0 if unused
  std::string core_version{"bruce_v6"}; // Version tag for compatibility checks
  std::vector<val_t> vec;
};

}
