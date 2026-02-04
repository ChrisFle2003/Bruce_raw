#include "bruce/io/project.hpp"
#include "bruce/io/minijson.hpp"
#include "bruce/observer/cli_usage.hpp"
#include "bruce/core/ops/ops.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <sstream>
#include <regex>
#include <filesystem>

using bruce::io::Project;

static void print_vec(const bruce::core::AxisRegistry &axes, const std::vector<bruce::val_t> &v)
{
  for (bruce::dim_t i = 0; i < axes.n; ++i)
  {
    const auto &lab = axes.labels[i];
    std::cout << "  " << lab.negative << "↔" << lab.positive << " : " << (int)v[i] << "\n";
  }
}

static std::string join_path(const std::string &a, const std::string &b)
{
#ifdef _WIN32
  const char sep = '\\';
#else
  const char sep = '/';
#endif
  if (!a.empty() && a.back() == sep)
    return a + b;
  return a + sep + b;
}

static std::string json_escape(const std::string &s)
{
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s)
  {
    if (c == '\\')
      out += "\\\\";
    else if (c == '\"')
      out += "\\\"";
    else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else
      out.push_back(c);
  }
  return out;
}

static std::string read_all(const std::string &path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw std::runtime_error("cannot open file: " + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

static std::string to_lower_ascii(std::string s)
{
  for (char &c : s)
    c = (char)std::tolower((unsigned char)c);
  return s;
}

static std::vector<std::string> split_ws_lower(const std::string &text)
{
  std::vector<std::string> out;
  std::string cur;
  for (char c : text)
  {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
    {
      if (!cur.empty())
      {
        out.push_back(to_lower_ascii(cur));
        cur.clear();
      }
    }
    else
    {
      cur.push_back(c);
    }
  }
  if (!cur.empty())
    out.push_back(to_lower_ascii(cur));
  return out;
}

static inline std::int64_t l1_dist(const std::vector<bruce::val_t> &a,
                                   const std::vector<bruce::val_t> &b)
{
  std::int64_t s = 0;
  for (std::size_t i = 0; i < a.size(); ++i)
  {
    auto d = (int)a[i] - (int)b[i];
    if (d < 0)
      d = -d;
    s += d;
  }
  return s;
}

// ---- Mini JSON parsing für v1 (Regex-basiert, genau für unsere Files) ----

static std::unordered_map<std::string, bruce::state_id_t>
parse_lexicon_term_to_state(const std::string &json)
{
  std::unordered_map<std::string, bruce::state_id_t> m;

  // match: {"term":"do","state_id":1}
  std::regex re(
      R"regex(\{\s*"term"\s*:\s*"([^"]+)"\s*,\s*"state_id"\s*:\s*([0-9]+)\s*\})regex");

  for (auto it = std::sregex_iterator(json.begin(), json.end(), re);
       it != std::sregex_iterator(); ++it)
  {
    std::string term = (*it)[1].str();
    auto id = static_cast<bruce::state_id_t>(std::stoull((*it)[2].str()));
    m[to_lower_ascii(term)] = id;
  }
  return m;
}

static std::unordered_map<int, std::string>
parse_actions_id_to_name(const std::string &json)
{
  std::unordered_map<int, std::string> m;

  // match: {"action_id":1,"name":"DO_NOW"}
  std::regex re(
      R"regex(\{\s*"action_id"\s*:\s*([0-9]+)\s*,\s*"name"\s*:\s*"([^"]+)"\s*\})regex");

  for (auto it = std::sregex_iterator(json.begin(), json.end(), re);
       it != std::sregex_iterator(); ++it)
  {
    int id = std::stoi((*it)[1].str());
    std::string name = (*it)[2].str();
    m[id] = name;
  }
  return m;
}

static int parse_policy_default_action(const std::string &json)
{
  std::regex re(R"regex("default_action_id"\s*:\s*([0-9]+))regex");
  std::smatch m;
  if (std::regex_search(json, m, re))
    return std::stoi(m[1].str());
  return 0;
}

static std::unordered_map<bruce::state_id_t, int>
parse_policy_state_to_action(const std::string &json)
{
  std::unordered_map<bruce::state_id_t, int> m;

  // match: {"state_id":1,"action_id":2}
  std::regex re(
      R"regex(\{\s*"state_id"\s*:\s*([0-9]+)\s*,\s*"action_id"\s*:\s*([0-9]+)\s*\})regex");

  for (auto it = std::sregex_iterator(json.begin(), json.end(), re);
       it != std::sregex_iterator(); ++it)
  {
    auto sid = (bruce::state_id_t)std::stoull((*it)[1].str());
    int aid = std::stoi((*it)[2].str());
    m[sid] = aid;
  }
  return m;
}

// ---- Multi-lexicon helper ----

static std::vector<std::string>
list_lexicon_files(const std::string &project_dir)
{
  namespace fs = std::filesystem;
  std::vector<std::string> files;

  fs::path dir = fs::path(project_dir) / "lexicon";
  if (!fs::exists(dir) || !fs::is_directory(dir))
    return files;

  for (const auto &e : fs::directory_iterator(dir))
  {
    if (!e.is_regular_file())
      continue;
    auto p = e.path();
    if (p.extension() == ".json")
      files.push_back(p.string());
  }

  std::sort(files.begin(), files.end()); // deterministisch
  return files;
}

static std::vector<std::string> load_manifest_files(const std::string &path)
{
  using namespace bruce::io::minijson;
  auto root = parse_file(path);
  auto arr = root.at("files");
  if (arr.type != Value::Type::Array)
    throw std::runtime_error("manifest.json: files must be array");
  std::vector<std::string> out;
  for (auto &v : arr.a)
    out.push_back(v.s);
  return out;
}

static int dataset_fields_check(const std::string &project_dir)
{
  namespace fs = std::filesystem;
  bool ok = true;

  const std::string fields_dir = join_path(project_dir, "fields");
  const std::string fields_manifest = join_path(fields_dir, "manifest.json");
  if (fs::exists(fields_manifest))
  {
    std::cout << "fields/manifest.json: ok\n";
    auto files = load_manifest_files(fields_manifest);
    for (const auto &rel : files)
    {
      std::string full = join_path(fields_dir, rel);
      bool exists = fs::exists(full);
      std::cout << "fields/" << rel << ": " << (exists ? "ok" : "MISSING") << "\n";
      if (!exists)
        ok = false;
    }
  }
  else
  {
    std::cout << "fields/manifest.json: MISSING\n";
    ok = false;
  }

  const std::string proj_dir = join_path(project_dir, "projections");
  const std::string proj_manifest = join_path(proj_dir, "manifest.json");
  if (fs::exists(proj_manifest))
  {
    std::cout << "projections/manifest.json: ok\n";
    auto files = load_manifest_files(proj_manifest);
    for (const auto &rel : files)
    {
      std::string full = join_path(proj_dir, rel);
      bool exists = fs::exists(full);
      std::cout << "projections/" << rel << ": " << (exists ? "ok" : "MISSING") << "\n";
      if (!exists)
        ok = false;
    }
  }
  else
  {
    std::cout << "projections/manifest.json: MISSING\n";
    ok = false;
  }

  return ok ? 0 : 2;
}

int main(int argc, char **argv)
{
  try
  {
    if (argc < 4)
    {
      std::cout << bruce::observer::usage_text;
      return 1;
    }
    std::string project_dir;
    int i = 1;
    if (std::string(argv[i]) == "--project")
    {
      project_dir = argv[i + 1];
      i += 2;
    }
    else
    {
      std::cout << bruce::observer::usage_text;
      return 1;
    }
    if (i >= argc)
    {
      std::cout << bruce::observer::usage_text;
      return 1;
    }

    std::string cmd = argv[i++];

    if (cmd == "dataset")
    {
      if (i >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      std::string sub = argv[i++];
      if (sub == "fields-check")
      {
        return dataset_fields_check(project_dir);
      }
      std::cout << bruce::observer::usage_text;
      return 1;
    }

    Project p = bruce::io::load_project(project_dir);
    auto axes = p.axes.view();

    if (cmd == "info")
    {
      std::cout << "Bruce v4\n";
      std::cout << "Axes: " << axes.n << "\n";
      std::cout << "States: " << p.state_store.size() << " (last_id=" << p.state_store.last_id() << ")\n";
      std::cout << "Rules: " << p.rules.size() << "\n";
      return 0;
    }

    if (cmd == "validate")
    {
      std::cout << "OK\n";
      return 0;
    }

    if (cmd == "list-states")
    {
      auto ids = p.state_store.ids();
      for (auto id : ids)
        std::cout << id << "\n";
      return 0;
    }

    if (cmd == "new-state")
    {
      if (i >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      const bruce::core_id_t core_id = (bruce::core_id_t)std::stoll(argv[i++]);
      if (i + (int)axes.n > argc)
      {
        std::cerr << "Expected " << axes.n << " vector values\n";
        return 2;
      }
      std::vector<bruce::val_t> v(axes.n);
      for (bruce::dim_t k = 0; k < axes.n; ++k)
      {
        int x = std::stoi(argv[i++]);
        v[k] = axes.clamp(k, x);
      }
      const bruce::state_id_t new_id = p.state_store.last_id() + 1;
      const std::string patch_path = join_path(join_path(project_dir, "changes"), "patches.jsonl");
      std::ofstream out(patch_path, std::ios::binary | std::ios::app);
      if (!out)
      {
        std::cerr << "Cannot open patches file: " << patch_path << "\n";
        return 3;
      }
      out << "{\"op\":\"add_state\",\"id\":" << new_id << ",\"core_id\":" << (int)core_id << ",\"vector\":[";
      for (bruce::dim_t k = 0; k < axes.n; ++k)
      {
        if (k)
          out << ",";
        out << (int)v[k];
      }
      out << "]}\n";
      std::cout << "Appended add_state patch. New StateID: " << new_id << "\n";
      return 0;
    }

    if (cmd == "learn-add-rule")
    {
      if (i >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      const std::string name = argv[i++];
      if (i + (int)axes.n > argc)
      {
        std::cerr << "Expected " << axes.n << " delta thresholds\n";
        return 2;
      }
      std::vector<int> d(axes.n);
      for (bruce::dim_t k = 0; k < axes.n; ++k)
      {
        int x = std::stoi(argv[i++]);
        if (x < 0)
          x = -x;
        d[k] = x;
      }
      bruce::rule_id_t max_id = 0;
      for (const auto &r : p.rules.all())
        max_id = std::max(max_id, r.id);
      const bruce::rule_id_t new_id = max_id + 1;

      const std::string patch_path = join_path(join_path(project_dir, "changes"), "patches.jsonl");
      std::ofstream out(patch_path, std::ios::binary | std::ios::app);
      if (!out)
      {
        std::cerr << "Cannot open patches file: " << patch_path << "\n";
        return 3;
      }
      out << "{\"op\":\"add_rule\",\"id\":" << new_id << ",\"name\":\"" << json_escape(name) << "\",\"max_abs_delta\":[";
      for (bruce::dim_t k = 0; k < axes.n; ++k)
      {
        if (k)
          out << ",";
        out << d[k];
      }
      out << "]}\n";
      std::cout << "Appended add_rule patch. New RuleID: " << new_id << "\n";
      return 0;
    }

    if (cmd == "show-state")
    {
      if (i >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      auto id = (bruce::state_id_t)std::stoull(argv[i++]);
      const auto *s = p.state_store.get(id);
      if (!s)
      {
        std::cerr << "Unknown state id\n";
        return 2;
      }
      std::cout << "StateID: " << s->id << "\n";
      std::cout << "CoreID: " << s->core_id;
      auto it = p.core_name.find(s->core_id);
      if (it != p.core_name.end())
        std::cout << " (" << it->second << ")";
      std::cout << "\n";
      std::cout << "Vector:\n";
      print_vec(axes, s->vec);
      std::cout << "Collapse-to-neutral: " << bruce::core::ops::collapse_to_neutral(s->vec, axes) << "\n";
      return 0;
    }

    if (cmd == "mix")
    {
      if (i + 1 >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      auto a_id = (bruce::state_id_t)std::stoull(argv[i++]);
      auto b_id = (bruce::state_id_t)std::stoull(argv[i++]);
      const auto *a = p.state_store.get(a_id);
      const auto *b = p.state_store.get(b_id);
      if (!a || !b)
      {
        std::cerr << "Unknown state id(s)\n";
        return 2;
      }
      if (a->core_id != b->core_id)
      {
        std::cerr << "Core mismatch (strict)\n";
        return 3;
      }
      std::vector<bruce::val_t> outv(axes.n, 0);
      bruce::core::ops::mix_into(a->vec, b->vec, axes, outv);
      std::cout << "Result vector:\n";
      print_vec(axes, outv);
      std::cout << "Collapse-to-neutral: " << bruce::core::ops::collapse_to_neutral(outv, axes) << "\n";
      return 0;
    }

    if (cmd == "distance")
    {
      if (i + 1 >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      auto a_id = (bruce::state_id_t)std::stoull(argv[i++]);
      auto b_id = (bruce::state_id_t)std::stoull(argv[i++]);
      const auto *a = p.state_store.get(a_id);
      const auto *b = p.state_store.get(b_id);
      if (!a || !b)
      {
        std::cerr << "Unknown state id(s)\n";
        return 2;
      }
      if (a->core_id != b->core_id)
      {
        std::cerr << "Core mismatch (strict)\n";
        return 3;
      }
      auto d = bruce::core::ops::l1_distance(a->vec, b->vec);
      std::cout << "L1 distance: " << d << "\n";
      return 0;
    }

    if (cmd == "allowed")
    {
      if (i + 1 >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      auto from_id = (bruce::state_id_t)std::stoull(argv[i++]);
      auto to_id = (bruce::state_id_t)std::stoull(argv[i++]);
      const auto *from = p.state_store.get(from_id);
      const auto *to = p.state_store.get(to_id);
      if (!from || !to)
      {
        std::cerr << "Unknown state id(s)\n";
        return 2;
      }
      auto ok = p.rules.allows(*from, *to, axes);
      if (ok.empty())
      {
        std::cout << "Not allowed by any rule.\n";
        return 0;
      }
      std::cout << "Allowed by RuleIDs: ";
      for (std::size_t k = 0; k < ok.size(); ++k)
      {
        if (k)
          std::cout << ",";
        std::cout << ok[k];
      }
      std::cout << "\n";
      return 0;
    }

    if (cmd == "dump")
    {
      std::string out_path;
      if (i + 1 <= argc && i < argc && std::string(argv[i]) == "--out")
      {
        out_path = argv[i + 1];
      }
      if (out_path.empty())
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      bruce::io::export_json(out_path, p);
      std::cout << "Wrote: " << out_path << "\n";
      return 0;
    }

    if (cmd == "fields")
    {
      if (i >= argc)
      {
        std::cout << bruce::observer::usage_text;
        return 1;
      }
      std::string sub = argv[i++];
      if (sub == "list")
      {
        for (const auto &field : p.fields)
        {
          std::cout << field.id << " core_id=" << field.selector.core_id
                    << " x_proj=" << field.x_proj << " y_proj=" << field.y_proj << "\n";
        }
        return 0;
      }
      if (sub == "build")
      {
        if (i >= argc)
        {
          std::cout << bruce::observer::usage_text;
          return 1;
        }
        int field_id = std::stoi(argv[i++]);
        const auto *field = bruce::io::find_field(p, field_id);
        if (!field)
        {
          std::cerr << "Unknown field id\n";
          return 2;
        }
        auto cache = bruce::io::build_field_cache(p, *field);
        std::cout << "Field " << cache.field_id << " grid " << cache.grid << "x" << cache.grid << "\n";
        std::size_t max_cell = 0;
        std::cout << "Occupancy:\n";
        for (std::uint32_t y = 0; y < cache.grid; ++y)
        {
          for (std::uint32_t x = 0; x < cache.grid; ++x)
          {
            std::size_t count = cache.cells[y][x].size();
            max_cell = std::max(max_cell, count);
            std::cout << count;
            if (x + 1 < cache.grid)
              std::cout << " ";
          }
          std::cout << "\n";
        }
        std::cout << "Max cell size: " << max_cell << "\n";
        std::cout << "Resonance:\n";
        for (std::uint32_t y = 0; y < cache.grid; ++y)
        {
          for (std::uint32_t x = 0; x < cache.grid; ++x)
          {
            std::cout << (int)cache.resonance[y][x];
            if (x + 1 < cache.grid)
              std::cout << " ";
          }
          std::cout << "\n";
        }
        return 0;
      }
      std::cout << bruce::observer::usage_text;
      return 1;
    }

    if (cmd == "route")
    {
      // parse args: --text "<...>"  --lexicon <file> (kann mehrfach vorkommen)
      std::string text;
      std::vector<std::string> lex_files;

      while (i < argc)
      {
        std::string a = argv[i++];
        if (a == "--text" && i < argc)
        {
          text = argv[i++];
        }
        else if (a == "--lexicon" && i < argc)
        {
          lex_files.push_back(argv[i++]);
        }
        else
        {
          // unknown args ignore
        }
      }

      if (text.empty())
      {
        std::cerr << "route: missing --text\n";
        return 2;
      }

      // Default: alle project/lexicon/*.json
      if (lex_files.empty())
      {
        lex_files = list_lexicon_files(project_dir);
        if (lex_files.empty())
        {
          std::cerr << "route: no lexicon files found in "
                    << join_path(project_dir, "lexicon") << "\n";
          return 2;
        }
      }

      // load router files (actions/policy wie gehabt)
      const std::string act_path = join_path(join_path(project_dir, "router"), "actions.json");
      const std::string pol_path = join_path(join_path(project_dir, "router"), "policy.json");

      auto act_json = read_all(act_path);
      auto pol_json = read_all(pol_path);

      auto action_id_to_name = parse_actions_id_to_name(act_json);
      int default_action = parse_policy_default_action(pol_json);
      auto state_to_action = parse_policy_state_to_action(pol_json);

      // load + merge lexica (spätere überschreiben frühere)
      std::unordered_map<std::string, bruce::state_id_t> term_to_state;
      for (const auto &lf : lex_files)
      {
        auto lex_json = read_all(lf);
        auto one = parse_lexicon_term_to_state(lex_json);
        for (auto &kv : one)
          term_to_state[kv.first] = kv.second;
      }

      // tokenize
      auto tokens = split_ws_lower(text);

      // map to state ids (unknown ignore)
      struct Used
      {
        std::string tok;
        bruce::state_id_t id;
      };
      std::vector<Used> used;
      used.reserve(tokens.size());

      for (auto &t : tokens)
      {
        auto it = term_to_state.find(t);
        if (it != term_to_state.end())
          used.push_back({t, it->second});
      }

      // no known tokens => default
      if (used.empty())
      {
        std::string aname = action_id_to_name.count(default_action) ? action_id_to_name[default_action] : "UNKNOWN_ACTION";

        std::cout << "{"
                  << "\"input\":\"" << json_escape(text) << "\","
                  << "\"lexicons\":[";
        for (size_t k = 0; k < lex_files.size(); ++k)
        {
          if (k)
            std::cout << ",";
          std::cout << "\"" << json_escape(lex_files[k]) << "\"";
        }
        std::cout << "],"
                  << "\"tokens\":[";
        for (size_t k = 0; k < tokens.size(); ++k)
        {
          if (k)
            std::cout << ",";
          std::cout << "\"" << json_escape(tokens[k]) << "\"";
        }
        std::cout << "],"
                  << "\"used\":[],"
                  << "\"chosen_state_id\":null,"
                  << "\"action\":{\"action_id\":" << default_action << ",\"name\":\"" << json_escape(aname) << "\"},"
                  << "\"why\":{\"reason\":\"no known tokens\"}"
                  << "}\n";
        return 0;
      }

      // build query vector via mix
      const auto *s0 = p.state_store.get(used[0].id);
      if (!s0)
      {
        std::cerr << "route: lexicon references unknown state_id " << used[0].id << "\n";
        return 3;
      }

      const bruce::core_id_t core_id = s0->core_id;
      std::vector<bruce::val_t> q = s0->vec;
      std::vector<bruce::val_t> tmp(axes.n, 0);

      for (size_t u = 1; u < used.size(); ++u)
      {
        const auto *sx = p.state_store.get(used[u].id);
        if (!sx)
          continue; // ignore broken refs
        if (sx->core_id != core_id)
          continue; // strict core (deterministisch)
        bruce::core::ops::mix_into(q, sx->vec, axes, tmp);
        q.swap(tmp);
      }

      // nearest within same core
      auto ids = p.state_store.ids();
      bruce::state_id_t best_id = 0;
      std::int64_t best_d = (std::int64_t)9e18;

      for (auto sid : ids)
      {
        const auto *s = p.state_store.get(sid);
        if (!s)
          continue;
        if (s->core_id != core_id)
          continue;
        std::int64_t d = l1_dist(q, s->vec);
        if (d < best_d || (d == best_d && sid < best_id))
        {
          best_d = d;
          best_id = sid;
        }
      }

      // map to action
      int action_id = default_action;
      auto itA = state_to_action.find(best_id);
      if (itA != state_to_action.end())
        action_id = itA->second;

      std::string action_name = action_id_to_name.count(action_id) ? action_id_to_name[action_id] : "UNKNOWN_ACTION";

      // output JSON
      std::cout << "{"
                << "\"input\":\"" << json_escape(text) << "\","
                << "\"lexicons\":[";
      for (size_t k = 0; k < lex_files.size(); ++k)
      {
        if (k)
          std::cout << ",";
        std::cout << "\"" << json_escape(lex_files[k]) << "\"";
      }
      std::cout << "],"
                << "\"tokens\":[";
      for (size_t k = 0; k < tokens.size(); ++k)
      {
        if (k)
          std::cout << ",";
        std::cout << "\"" << json_escape(tokens[k]) << "\"";
      }
      std::cout << "],"
                << "\"used\":[";
      for (size_t k = 0; k < used.size(); ++k)
      {
        if (k)
          std::cout << ",";
        std::cout << "{\"token\":\"" << json_escape(used[k].tok) << "\",\"state_id\":" << (unsigned long long)used[k].id << "}";
      }
      std::cout << "],"
                << "\"core_id\":" << (int)core_id << ","
                << "\"query_vector\":[";
      for (bruce::dim_t k = 0; k < axes.n; ++k)
      {
        if (k)
          std::cout << ",";
        std::cout << (int)q[k];
      }
      std::cout << "],"
                << "\"chosen_state_id\":" << (unsigned long long)best_id << ","
                << "\"distance_l1\":" << best_d << ","
                << "\"action\":{\"action_id\":" << action_id << ",\"name\":\"" << json_escape(action_name) << "\"},"
                << "\"why\":{"
                << "\"nearest\":\"chosen = argmin_s sum_i abs(q[i]-s[i])\","
                << "\"tie_break\":\"lowest state_id\""
                << "}"
                << "}\n";

      return 0;
    }

    std::cout << bruce::observer::usage_text;
    return 1;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 10;
  }
}
