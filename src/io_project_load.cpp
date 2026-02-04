#include "bruce/io/project.hpp"
#include "bruce/io/minijson.hpp"
#include "bruce/util/sha256.hpp"
#include <stdexcept>
#include <fstream>

namespace bruce::io {

static std::string join_path(const std::string& a, const std::string& b){
#ifdef _WIN32
  const char sep='\\';
#else
  const char sep='/';
#endif
  if(!a.empty() && a.back()==sep) return a+b;
  return a + sep + b;
}

static void validate_project_meta(const std::string& project_dir){
  const std::string ver = join_path(join_path(project_dir, "meta"), "schema_version.txt");
  std::ifstream in(ver, std::ios::binary);
  if (!in.good()) {
    // Recommended but optional for now
    return;
  }
  std::string s;
  std::getline(in, s);
  const std::string expected = "bruce_v4_project_v1";
  if (s != expected) {
    throw std::runtime_error("meta/schema_version.txt mismatch (expected '" + expected + "')");
  }
}

static bruce::io::View2DConfig load_view2d_optional(const std::string& project_dir, const bruce::core::AxisRegistry& axes){
  bruce::io::View2DConfig cfg;
  const std::string path = join_path(join_path(project_dir, "meta"), "viewer.json");
  std::ifstream test(path, std::ios::binary);
  if (!test.good()) {
    // default: axis 0 vs axis 1 (if available)
    cfg.x_axis = 0;
    cfg.y_axis = (axes.n > 1) ? 1 : 0;
    cfg.grid = 5;
    return cfg;
  }
  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  if (root.type != Value::Type::Object) throw std::runtime_error("meta/viewer.json: must be object");

  if (root.o.find("x_axis") != root.o.end()) cfg.x_axis = (bruce::dim_t)root.at("x_axis").i;
  if (root.o.find("y_axis") != root.o.end()) cfg.y_axis = (bruce::dim_t)root.at("y_axis").i;
  if (root.o.find("grid") != root.o.end()) cfg.grid = (std::uint32_t)root.at("grid").i;

  if (cfg.grid == 0) cfg.grid = 5;
  if (cfg.x_axis >= axes.n || cfg.y_axis >= axes.n) throw std::runtime_error("meta/viewer.json: axis index out of range");
  return cfg;
}

static bruce::core::OwnedAxes load_axes(const std::string& path){
  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  auto scale = root.at("scale");
  int smin = (int)scale.at("min").i;
  int smax = (int)scale.at("max").i;
  auto axes_arr = root.at("axes");
  if (axes_arr.type != Value::Type::Array) throw std::runtime_error("axes.json: axes must be array");

  std::size_t n = axes_arr.a.size();
  if (n == 0) throw std::runtime_error("axes.json: no axes");

  bruce::core::OwnedAxes ax;
  ax.minv.assign(n, (bruce::val_t)smin);
  ax.maxv.assign(n, (bruce::val_t)smax);
  ax.max_abs.resize(n);
  ax.labels.resize(n);

  int ma = std::max(std::abs(smin), std::abs(smax));
  for (std::size_t i=0;i<n;++i){
    auto& a = axes_arr.a[i];
    ax.labels[i].negative = a.at("negative").s;
    ax.labels[i].positive = a.at("positive").s;
    ax.max_abs[i] = (bruce::val_t)ma;
  }
  return ax;
}

static void load_cores_optional(const std::string& path, std::unordered_map<bruce::core_id_t, std::string>& out){
  out.clear();
  std::ifstream test(path, std::ios::binary);
  if (!test.good()) return;

  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  auto arr = root.at("cores");
  if (arr.type != Value::Type::Array) throw std::runtime_error("cores.json: cores must be array");
  for (auto& c : arr.a){
    bruce::core_id_t id = (bruce::core_id_t)c.at("id").i;
    std::string name = c.at("name").s;
    out[id] = std::move(name);
  }
}

static void load_states_file(const std::string& path, const bruce::core::AxisRegistry& axes, bruce::core::StateStore& store){
  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  auto arr = root.at("states");
  if (arr.type != Value::Type::Array) throw std::runtime_error("states json: states must be array");

  for (auto& s : arr.a){
    bruce::state_id_t id = 0;
    if (s.o.find("id") != s.o.end()) id = (bruce::state_id_t)s.at("id").i;

    bruce::core_id_t core_id = (bruce::core_id_t)s.at("core_id").i;
    std::string core_version = "bruce_v6";
    if (s.o.find("core_version") != s.o.end()) core_version = s.at("core_version").s;
    auto vec_arr = s.at("vector");
    if (vec_arr.type != Value::Type::Array) throw std::runtime_error("state.vector must be array");
    if (vec_arr.a.size() != axes.n) throw std::runtime_error("state.vector length mismatch axes");

    std::vector<bruce::val_t> v(axes.n);
    for (bruce::dim_t i=0;i<axes.n;++i){
      int x = (int)vec_arr.a[i].i;
      v[i] = axes.clamp(i, x);
    }

    if (id != 0) {
      if (!store.insert(id, core_id, std::move(v), core_version)) {
        throw std::runtime_error("states: duplicate or non-monotonic state id");
      }
    } else {
      store.create(core_id, std::move(v), core_version);
    }
  }
}

static std::vector<std::string> load_manifest_required(const std::string& path){
  std::ifstream test(path, std::ios::binary);
  if (!test.good()) {
    throw std::runtime_error("manifest.json missing: " + path);
  }

  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  auto arr = root.at("files");
  if (arr.type != Value::Type::Array) throw std::runtime_error("manifest.json: files must be array");

  std::vector<std::string> out;
  for (auto& v : arr.a) out.push_back(v.s);
  return out;
}

static void load_rules(const std::string& path, const bruce::core::AxisRegistry& axes, bruce::core::RuleSet& rules){
  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  if (root.o.find("core_version") != root.o.end()) {
    rules.core_version = root.at("core_version").s;
  }
  auto arr = root.at("rules");
  if (arr.type != Value::Type::Array) throw std::runtime_error("rules json: rules must be array");

  for (auto& r : arr.a){
    bruce::core::Rule rule;
    rule.id = (bruce::rule_id_t)r.at("id").i;
    rule.name = r.at("name").s;
    auto mad = r.at("max_abs_delta");
    if (mad.type != Value::Type::Array) throw std::runtime_error("rule.max_abs_delta must be array");
    if (mad.a.size() != axes.n) throw std::runtime_error("rule.max_abs_delta length mismatch axes");

    rule.max_abs_delta.resize(axes.n);
    for (bruce::dim_t i=0;i<axes.n;++i){
      int x = (int)mad.a[i].i;
      if (x < 0) x = -x;
      rule.max_abs_delta[i] = (bruce::val_t)x;
    }
    rules.add(std::move(rule));
  }
}

static void load_projections_file(const std::string& path, const bruce::core::AxisRegistry& axes, Project& p) {
  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  auto arr = root.at("projections");
  if (arr.type != Value::Type::Array) throw std::runtime_error("projections json: projections must be array");

  for (auto& pr : arr.a) {
    ProjectionRule proj;
    proj.id = (int)pr.at("id").i;
    if (pr.o.find("name") != pr.o.end()) proj.name = pr.at("name").s;
    auto weights = pr.at("weights");
    if (weights.type != Value::Type::Array) throw std::runtime_error("projection.weights must be array");
    if (weights.a.size() != axes.n) throw std::runtime_error("projection.weights length mismatch axes");
    proj.weights.resize(weights.a.size());
    for (std::size_t i = 0; i < weights.a.size(); ++i) {
      proj.weights[i] = (std::int32_t)weights.a[i].i;
    }
    proj.shift = (int)pr.at("shift").i;
    if (proj.shift < 0 || proj.shift > 30) throw std::runtime_error("projection.shift out of range");
    proj.clamp_min = (int)pr.at("clamp_min").i;
    proj.clamp_max = (int)pr.at("clamp_max").i;
    if (proj.clamp_min > proj.clamp_max) throw std::runtime_error("projection clamp_min > clamp_max");

    if (p.projection_index.find(proj.id) != p.projection_index.end()) {
      throw std::runtime_error("projection id duplicate: " + std::to_string(proj.id));
    }
    p.projection_index[proj.id] = p.projections.size();
    p.projections.push_back(std::move(proj));
  }
}

static void load_field_file(const std::string& path, Project& p) {
  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  if (root.type != Value::Type::Object) throw std::runtime_error("field json: must be object");

  FieldDef field;
  field.id = (int)root.at("id").i;
  if (root.o.find("name") != root.o.end()) field.name = root.at("name").s;
  auto selector = root.at("selector");
  if (selector.type != Value::Type::Object) throw std::runtime_error("field.selector must be object");
  field.selector.core_id = (bruce::core_id_t)selector.at("core_id").i;
  if ((int)field.selector.core_id < 0) throw std::runtime_error("field.selector.core_id must be >= 0");
  field.x_proj = (int)root.at("x_proj").i;
  field.y_proj = (int)root.at("y_proj").i;
  field.grid = (std::uint32_t)root.at("grid").i;
  if (field.grid != 5) throw std::runtime_error("field.grid must be 5");
  if (p.projection_index.find(field.x_proj) == p.projection_index.end()) {
    throw std::runtime_error("field x_proj missing: " + std::to_string(field.x_proj));
  }
  if (p.projection_index.find(field.y_proj) == p.projection_index.end()) {
    throw std::runtime_error("field y_proj missing: " + std::to_string(field.y_proj));
  }

  if (p.field_index.find(field.id) != p.field_index.end()) {
    throw std::runtime_error("field id duplicate: " + std::to_string(field.id));
  }
  p.field_index[field.id] = p.fields.size();
  p.fields.push_back(std::move(field));
}

static void hash_file_or_throw(const std::string& path, bruce::util::Sha256& sha) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("missing file for hash: " + path);
  char buffer[4096];
  while (in) {
    in.read(buffer, sizeof(buffer));
    std::streamsize n = in.gcount();
    if (n > 0) sha.update(reinterpret_cast<const std::uint8_t*>(buffer), static_cast<std::size_t>(n));
  }
}

static void hash_file_optional(const std::string& path, bruce::util::Sha256& sha) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return;
  char buffer[4096];
  while (in) {
    in.read(buffer, sizeof(buffer));
    std::streamsize n = in.gcount();
    if (n > 0) sha.update(reinterpret_cast<const std::uint8_t*>(buffer), static_cast<std::size_t>(n));
  }
}

static std::string compute_build_hash(const std::string& project_dir,
                                      const std::vector<std::string>& states_manifest,
                                      const std::vector<std::string>& rules_manifest,
                                      const std::vector<std::string>& projections_manifest,
                                      const std::vector<std::string>& fields_manifest) {
  bruce::util::Sha256 sha;
  const std::string axes_path = join_path(join_path(project_dir, "axes"), "axes.json");
  hash_file_or_throw(axes_path, sha);

  const std::string states_dir = join_path(project_dir, "states");
  hash_file_or_throw(join_path(states_dir, "manifest.json"), sha);
  for (const auto& rel : states_manifest) {
    hash_file_or_throw(join_path(states_dir, rel), sha);
  }

  const std::string rules_dir = join_path(project_dir, "rules");
  hash_file_or_throw(join_path(rules_dir, "manifest.json"), sha);
  for (const auto& rel : rules_manifest) {
    hash_file_or_throw(join_path(rules_dir, rel), sha);
  }

  const std::string proj_dir = join_path(project_dir, "projections");
  hash_file_or_throw(join_path(proj_dir, "manifest.json"), sha);
  for (const auto& rel : projections_manifest) {
    hash_file_or_throw(join_path(proj_dir, rel), sha);
  }

  const std::string fields_dir = join_path(project_dir, "fields");
  hash_file_or_throw(join_path(fields_dir, "manifest.json"), sha);
  for (const auto& rel : fields_manifest) {
    hash_file_or_throw(join_path(fields_dir, rel), sha);
  }

  const std::string patches = join_path(join_path(project_dir, "changes"), "patches.jsonl");
  hash_file_optional(patches, sha);

  return bruce::util::to_hex(sha.finalize());
}

static void apply_patches_optional(const std::string& project_dir, const bruce::core::AxisRegistry& axes, Project& p){
  const std::string patches = join_path(join_path(project_dir, "changes"), "patches.jsonl");
  std::ifstream in(patches, std::ios::binary);
  if (!in.good()) return;

  using namespace bruce::io::minijson;
  std::string line;
  while (std::getline(in, line)) {
    bool all_ws = true;
    for (char c: line) {
      if (c!=' ' && c!='\t' && c!='\r' && c!='\n') { all_ws = false; break; }
    }
    if (all_ws) continue;

    Value root = parse(line);
    const std::string op = root.at("op").s;

    if (op == "add_state") {
      bruce::state_id_t id = 0;
      if (root.o.find("id") != root.o.end()) id = (bruce::state_id_t)root.at("id").i;
      bruce::core_id_t core_id = (bruce::core_id_t)root.at("core_id").i;
      std::string core_version = "bruce_v6";
      if (root.o.find("core_version") != root.o.end()) core_version = root.at("core_version").s;

      auto vec_arr = root.at("vector");
      if (vec_arr.type != Value::Type::Array) throw std::runtime_error("patch add_state: vector must be array");
      if (vec_arr.a.size() != axes.n) throw std::runtime_error("patch add_state: vector length mismatch axes");

      std::vector<bruce::val_t> v(axes.n);
      for (bruce::dim_t i=0;i<axes.n;++i){
        int x = (int)vec_arr.a[i].i;
        v[i] = axes.clamp(i, x);
      }

      if (id != 0) {
        if (!p.state_store.insert(id, core_id, std::move(v), core_version)) {
          throw std::runtime_error("patch add_state: duplicate or non-monotonic state id");
        }
      } else {
        p.state_store.create(core_id, std::move(v), core_version);
      }

    } else if (op == "add_rule") {
      bruce::core::Rule rule;
      rule.id = 0;
      if (root.o.find("id") != root.o.end()) rule.id = (bruce::rule_id_t)root.at("id").i;
      rule.name = root.at("name").s;

      auto mad = root.at("max_abs_delta");
      if (mad.type != Value::Type::Array) throw std::runtime_error("patch add_rule: max_abs_delta must be array");
      if (mad.a.size() != axes.n) throw std::runtime_error("patch add_rule: max_abs_delta length mismatch axes");

      rule.max_abs_delta.resize(axes.n);
      for (bruce::dim_t i=0;i<axes.n;++i){
        int x = (int)mad.a[i].i;
        if (x < 0) x = -x;
        rule.max_abs_delta[i] = (bruce::val_t)x;
      }

      p.rules.add(std::move(rule));

    } else {
      throw std::runtime_error("unknown patch op: " + op);
    }
  }
}

Project load_project(const std::string& project_dir){
  Project p;
  validate_project_meta(project_dir);

  p.axes = load_axes(join_path(join_path(project_dir, "axes"), "axes.json"));
  auto axes = p.axes.view();

  // Observer-only 2D view config (never used by core math)
  p.view2d = load_view2d_optional(project_dir, axes);

  load_cores_optional(join_path(join_path(project_dir, "cores"), "cores.json"), p.core_name);

  // States: manifest.json required
  const std::string states_dir = join_path(project_dir, "states");
  const std::string states_manifest_path = join_path(states_dir, "manifest.json");
  auto states_manifest = load_manifest_required(states_manifest_path);
  for (const auto& rel : states_manifest) {
    load_states_file(join_path(states_dir, rel), axes, p.state_store);
  }

  // Rules: manifest.json required
  const std::string rules_dir = join_path(project_dir, "rules");
  const std::string rules_manifest_path = join_path(rules_dir, "manifest.json");
  auto rules_manifest = load_manifest_required(rules_manifest_path);
  for (const auto& rel : rules_manifest) {
    load_rules(join_path(rules_dir, rel), axes, p.rules);
  }

  // Projections: manifest.json required
  const std::string proj_dir = join_path(project_dir, "projections");
  const std::string proj_manifest_path = join_path(proj_dir, "manifest.json");
  auto proj_manifest = load_manifest_required(proj_manifest_path);
  for (const auto& rel : proj_manifest) {
    load_projections_file(join_path(proj_dir, rel), axes, p);
  }

  // Fields: manifest.json required
  const std::string fields_dir = join_path(project_dir, "fields");
  const std::string fields_manifest_path = join_path(fields_dir, "manifest.json");
  auto fields_manifest = load_manifest_required(fields_manifest_path);
  for (const auto& rel : fields_manifest) {
    load_field_file(join_path(fields_dir, rel), p);
  }

  apply_patches_optional(project_dir, axes, p);

  p.snapshot.schema_version = "bruce_v5_snapshot_v1";
  p.snapshot.epoch = 0;
  p.snapshot.build_hash = compute_build_hash(project_dir, states_manifest, rules_manifest, proj_manifest, fields_manifest);
  p.snapshot.counts.axes_n = axes.n;
  p.snapshot.counts.states_count = p.state_store.size();
  p.snapshot.counts.rules_count = p.rules.size();
  p.snapshot.counts.fields_count = p.fields.size();
  p.snapshot.counts.projections_count = p.projections.size();

  return p;
}

} // namespace bruce::io
