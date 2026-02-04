#include "bruce/core/state/state_store.hpp"
#include <algorithm>

namespace bruce::core {

state_id_t StateStore::create(core_id_t core_id, std::vector<val_t> vec, std::string core_version) {
  state_id_t id = ++last_id_;
  State s;
  s.id = id;
  s.core_id = core_id;
  s.core_version = std::move(core_version);
  s.vec = std::move(vec);
  states_.emplace(id, std::move(s));
  return id;
}

bool StateStore::insert(state_id_t id, core_id_t core_id, std::vector<val_t> vec, std::string core_version) {
  if (id == 0) return false;
  if (states_.find(id) != states_.end()) return false;
  if (id < last_id_ + 1) return false;
  State s;
  s.id = id;
  s.core_id = core_id;
  s.core_version = std::move(core_version);
  s.vec = std::move(vec);
  states_.emplace(id, std::move(s));
  last_id_ = id;
  return true;
}

const State* StateStore::get(state_id_t id) const {
  auto it = states_.find(id);
  if (it == states_.end()) return nullptr;
  return &it->second;
}

std::vector<state_id_t> StateStore::ids() const {
  std::vector<state_id_t> out;
  out.reserve(states_.size());
  for (const auto& kv : states_) out.push_back(kv.first);
  // Deterministic order for exports
  std::sort(out.begin(), out.end());
  return out;
}

} // namespace bruce::core
