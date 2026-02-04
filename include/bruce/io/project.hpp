#pragma once
#include "bruce/core/axes/axis_registry.hpp"
#include "bruce/core/state/state_store.hpp"
#include "bruce/core/rules/rule_set.hpp"
#include "bruce/core/transitions/transition.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace bruce::io {

// Observer-only configuration for 2D viewing/projection.
// The Bruce core never depends on this; it's used for dumps/visualization.
struct View2DConfig {
  bruce::dim_t x_axis = 0;
  bruce::dim_t y_axis = 0;
  std::uint32_t grid = 5; // 5x5 by default
};

struct ProjectionRule {
  int id = 0;
  std::string name;
  std::vector<std::int32_t> weights;
  int shift = 0;
  int clamp_min = 0;
  int clamp_max = 0;
};

struct FieldSelector {
  bruce::core_id_t core_id = 0;
};

struct FieldDef {
  int id = 0;
  std::string name;
  FieldSelector selector;
  int x_proj = 0;
  int y_proj = 0;
  std::uint32_t grid = 5;
};

struct FieldCache {
  int field_id = 0;
  int x_proj = 0;
  int y_proj = 0;
  std::uint32_t grid = 5;
  std::vector<std::vector<std::vector<bruce::state_id_t>>> cells;
  std::vector<std::vector<std::int8_t>> resonance;
};

struct SnapshotCounts {
  std::uint64_t axes_n = 0;
  std::uint64_t states_count = 0;
  std::uint64_t rules_count = 0;
  std::uint64_t fields_count = 0;
  std::uint64_t projections_count = 0;
};

struct Snapshot {
  std::string schema_version;
  std::uint64_t epoch = 0;
  std::string build_hash;
  SnapshotCounts counts;
};

struct Project {
  bruce::core::OwnedAxes axes;
  std::unordered_map<bruce::core_id_t, std::string> core_name; // observer-only
  View2DConfig view2d; // observer-only
  bruce::core::StateStore state_store;
  bruce::core::RuleSet rules;
  std::vector<bruce::core::Transition> transitions; // optional precomputed
  std::vector<ProjectionRule> projections;
  std::unordered_map<int, std::size_t> projection_index;
  std::vector<FieldDef> fields;
  std::unordered_map<int, std::size_t> field_index;
  Snapshot snapshot;
};

// Loads project/axes/axes.json, project/states/initial.json, project/rules/rules.json
Project load_project(const std::string& project_dir);

// Exports runtime to a single JSON (log/backup) for humans.
void export_json(const std::string& out_path, const Project& p);

const ProjectionRule* find_projection(const Project& p, int id);
const FieldDef* find_field(const Project& p, int id);
FieldCache build_field_cache(const Project& p, const FieldDef& field);
std::vector<FieldCache> build_all_field_caches(const Project& p);

}
