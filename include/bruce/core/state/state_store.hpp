#pragma once
#include "state.hpp"
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace bruce::core {

// StateStore owns immutable states and issues monotonically increasing IDs.
class StateStore {
public:
  state_id_t create(core_id_t core_id, std::vector<val_t> vec, std::string core_version = "bruce_v6");

  // Insert a state with an explicit ID (for loading from project files).
  // Deterministic constraints:
  //  - id must be > 0
  //  - id must be unique
  //  - id must be >= last_id()+1 (monotonic, no rewinding)
  // Returns false if constraints are violated.
  bool insert(state_id_t id, core_id_t core_id, std::vector<val_t> vec, std::string core_version = "bruce_v6");
  const State* get(state_id_t id) const;
  std::size_t size() const { return states_.size(); }
  state_id_t last_id() const { return last_id_; }

  // Snapshot helpers for observers/exporters (still deterministic).
  std::vector<state_id_t> ids() const;

private:
  state_id_t last_id_{0};
  std::unordered_map<state_id_t, State> states_;
};

}
