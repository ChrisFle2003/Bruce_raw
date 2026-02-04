#include "bruce/io/project.hpp"
#include "bruce/core/ops/ops.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace bruce::io {

static std::string esc(const std::string& s){
  std::string out; out.reserve(s.size()+8);
  for(char c: s){
    if(c=='\\') out += "\\\\";
    else if(c=='\"') out += "\\\"";
    else if(c=='\n') out += "\\n";
    else if(c=='\r') out += "\\r";
    else if(c=='\t') out += "\\t";
    else out.push_back(c);
  }
  return out;
}

void export_json(const std::string& out_path, const Project& p){
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if(!out) throw std::runtime_error("cannot open output json");

  auto axes = p.axes.view();

  out << "{\n";

  // Core labels (observer-only)
  out << "  \"cores\": [\n";
  {
    std::vector<std::pair<bruce::core_id_t, std::string>> cores;
    cores.reserve(p.core_name.size());
    for (const auto& kv : p.core_name) cores.push_back(kv);
    std::sort(cores.begin(), cores.end(), [](auto& a, auto& b){ return a.first < b.first; });
    for (std::size_t ci=0; ci<cores.size(); ++ci){
      out << "    {\"id\": " << cores[ci].first << ", \"name\": \"" << esc(cores[ci].second) << "\"}";
      if (ci+1<cores.size()) out << ",";
      out << "\n";
    }
  }
  out << "  ],\n";

  // Axes
  out << "  \"axes\": {\n";
  out << "    \"scale\": { \"min\": " << (int)axes.minv[0] << ", \"max\": " << (int)axes.maxv[0] << " },\n";
  out << "    \"items\": [\n";
  for (bruce::dim_t i=0;i<axes.n;++i){
    out << "      {\"negative\": \"" << esc(axes.labels[i].negative) << "\", \"positive\": \"" << esc(axes.labels[i].positive) << "\"}";
    if (i+1<axes.n) out << ",";
    out << "\n";
  }
  out << "    ]\n";
  out << "  },\n";

  // States
  out << "  \"states\": [\n";
  auto ids = p.state_store.ids();
  for (std::size_t idx=0; idx<ids.size(); ++idx){
    const auto* s = p.state_store.get(ids[idx]);
    out << "    {\"id\": " << s->id << ", \"core_id\": " << s->core_id
        << ", \"core_version\": \"" << esc(s->core_version) << "\", \"vector\": [";
    for (std::size_t j=0;j<s->vec.size();++j){
      if (j) out << ",";
      out << (int)s->vec[j];
    }
    out << "]}";
    if (idx+1<ids.size()) out << ",";
    out << "\n";
  }
  out << "  ],\n";

  // Rules
  out << "  \"rules\": [\n";
  const auto& rules = p.rules.all();
  for (std::size_t ri=0; ri<rules.size(); ++ri){
    const auto& r = rules[ri];
    out << "    {\"id\": " << r.id << ", \"name\": \"" << esc(r.name) << "\", \"max_abs_delta\": [";
    for (std::size_t j=0;j<r.max_abs_delta.size();++j){
      if (j) out << ",";
      out << (int)r.max_abs_delta[j];
    }
    out << "]}";
    if (ri+1<rules.size()) out << ",";
    out << "\n";
  }
  out << "  ],\n";
  out << "  \"rules_core_version\": \"" << esc(p.rules.core_version) << "\",\n";

  // Projections
  out << "  \"projections\": [\n";
  for (std::size_t pi = 0; pi < p.projections.size(); ++pi) {
    const auto& proj = p.projections[pi];
    out << "    {\"id\": " << proj.id << ", \"name\": \"" << esc(proj.name)
        << "\", \"shift\": " << proj.shift << ", \"clamp_min\": " << proj.clamp_min
        << ", \"clamp_max\": " << proj.clamp_max << ", \"weights\": [";
    for (std::size_t wi = 0; wi < proj.weights.size(); ++wi) {
      if (wi) out << ",";
      out << proj.weights[wi];
    }
    out << "]}";
    if (pi + 1 < p.projections.size()) out << ",";
    out << "\n";
  }
  out << "  ],\n";

  // Fields
  out << "  \"fields\": [\n";
  for (std::size_t fi = 0; fi < p.fields.size(); ++fi) {
    const auto& field = p.fields[fi];
    out << "    {\"id\": " << field.id;
    if (!field.name.empty()) out << ", \"name\": \"" << esc(field.name) << "\"";
    out << ", \"selector\": {\"core_id\": " << field.selector.core_id
        << "}, \"x_proj\": " << field.x_proj << ", \"y_proj\": " << field.y_proj
        << ", \"grid\": " << field.grid << "}";
    if (fi + 1 < p.fields.size()) out << ",";
    out << "\n";
  }
  out << "  ],\n";

  // BR2 caches (derived)
  const auto br2 = build_all_field_caches(p);
  out << "  \"br2\": [\n";
  for (std::size_t bi = 0; bi < br2.size(); ++bi) {
    const auto& cache = br2[bi];
    out << "    {\"field_id\": " << cache.field_id << ", \"x_proj\": " << cache.x_proj
        << ", \"y_proj\": " << cache.y_proj << ", \"grid\": " << cache.grid << ", \"cells\": [\n";
    for (std::uint32_t y = 0; y < cache.grid; ++y) {
      out << "      [";
      for (std::uint32_t x = 0; x < cache.grid; ++x) {
        if (x) out << ",";
        out << "[";
        const auto& lst = cache.cells[y][x];
        for (std::size_t k = 0; k < lst.size(); ++k) {
          if (k) out << ",";
          out << lst[k];
        }
        out << "]";
      }
      out << "]";
      if (y + 1 < cache.grid) out << ",";
      out << "\n";
    }
    out << "    ], \"resonance\": [\n";
    for (std::uint32_t y = 0; y < cache.grid; ++y) {
      out << "      [";
      for (std::uint32_t x = 0; x < cache.grid; ++x) {
        if (x) out << ",";
        out << (int)cache.resonance[y][x];
      }
      out << "]";
      if (y + 1 < cache.grid) out << ",";
      out << "\n";
    }
    out << "    ]}";
    if (bi + 1 < br2.size()) out << ",";
    out << "\n";
  }
  out << "  ],\n";

  // Snapshot
  out << "  \"snapshot\": {\n";
  out << "    \"schema_version\": \"" << esc(p.snapshot.schema_version) << "\",\n";
  out << "    \"epoch\": " << p.snapshot.epoch << ",\n";
  out << "    \"build_hash\": \"" << p.snapshot.build_hash << "\",\n";
  out << "    \"counts\": {\"axes_n\": " << p.snapshot.counts.axes_n
      << ", \"states_count\": " << p.snapshot.counts.states_count
      << ", \"rules_count\": " << p.snapshot.counts.rules_count
      << ", \"fields_count\": " << p.snapshot.counts.fields_count
      << ", \"projections_count\": " << p.snapshot.counts.projections_count << "}\n";
  out << "  }";

  // --- Viewer-friendly 2D export (observer-only) ---
  // This is a projection of high-D states onto a fixed grid for humans.
  // It never affects core computation.
  {
    const auto cfg = p.view2d;
    const std::uint32_t g = (cfg.grid == 0) ? 5u : cfg.grid;

    auto bin_1d = [&](int v, int lo, int hi) -> std::uint32_t {
      if (hi <= lo) return 0;
      // Map [lo..hi] into [0..g-1] deterministically.
      long long num = (long long)(v - lo) * (long long)g;
      long long den = (long long)(hi - lo + 1);
      long long b = num / den;
      if (b < 0) b = 0;
      if (b >= (long long)g) b = (long long)g - 1;
      return (std::uint32_t)b;
    };

    // Build grid cells: cells[y][x] -> list of state IDs
    std::vector<std::vector<std::vector<bruce::state_id_t>>> cells;
    cells.resize(g);
    for (std::uint32_t y=0;y<g;++y) cells[y].resize(g);

    auto ids2 = p.state_store.ids();
    for (auto sid : ids2) {
      const auto* s = p.state_store.get(sid);
      if (!s) continue;
      int vx = (int)s->vec[(bruce::dim_t)cfg.x_axis];
      int vy = (int)s->vec[(bruce::dim_t)cfg.y_axis];
      int lo = (int)axes.minv[0];
      int hi = (int)axes.maxv[0];
      std::uint32_t bx = bin_1d(vx, lo, hi);
      std::uint32_t by = bin_1d(vy, lo, hi);
      cells[by][bx].push_back(s->id);
    }

    out << ",\n  \"view2d\": {\n";
    out << "    \"x_axis\": " << cfg.x_axis << ",\n";
    out << "    \"y_axis\": " << cfg.y_axis << ",\n";
    out << "    \"grid\": " << g << ",\n";
    out << "    \"cells\": [\n";
    for (std::uint32_t y=0;y<g;++y){
      out << "      [";
      for (std::uint32_t x=0;x<g;++x){
        if (x) out << ",";
        out << "[";
        auto &lst = cells[y][x];
        std::sort(lst.begin(), lst.end());
        for (std::size_t k=0;k<lst.size();++k){
          if (k) out << ",";
          out << lst[k];
        }
        out << "]";
      }
      out << "]";
      if (y+1<g) out << ",";
      out << "\n";
    }
    out << "    ]\n";
    out << "  }\n";
  }

  // --- Transition explanation export (observer-only) ---
  // For each state, emit up to N allowed next-states with the rule(s) that allow it.
  // Deterministic ordering by (L1 distance, to_id).
  {
    const std::size_t MAX_EDGES_PER_FROM = 128;
    const auto ids3 = p.state_store.ids();
    out << ",\n  \"allowed\": {\n";
    out << "    \"formula\": \"allow(from,to,rule): for all axes i: |to[i]-from[i]| <= rule.max_abs_delta[i]\",\n";
    out << "    \"from\": [\n";

    for (std::size_t fi=0; fi<ids3.size(); ++fi){
      const auto* from = p.state_store.get(ids3[fi]);
      if (!from) continue;

      struct Edge { bruce::state_id_t to; std::vector<bruce::rule_id_t> rules; std::int64_t dist; };
      std::vector<Edge> edges;
      edges.reserve(ids3.size());

      for (auto tid : ids3){
        if (tid == from->id) continue;
        const auto* to = p.state_store.get(tid);
        if (!to) continue;
        auto ok = p.rules.allows(*from, *to, axes);
        if (ok.empty()) continue;
        std::int64_t d = bruce::core::ops::l1_distance(from->vec, to->vec);
        edges.push_back(Edge{to->id, std::move(ok), d});
      }

      std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b){
        if (a.dist != b.dist) return a.dist < b.dist;
        return a.to < b.to;
      });
      if (edges.size() > MAX_EDGES_PER_FROM) edges.resize(MAX_EDGES_PER_FROM);

      out << "      {\"from_id\": " << from->id << ", \"edges\": [";
      for (std::size_t ei=0; ei<edges.size(); ++ei){
        if (ei) out << ",";
        out << "{\"to_id\": " << edges[ei].to << ", \"l1\": " << edges[ei].dist << ", \"rule_ids\": [";
        auto rids = edges[ei].rules;
        std::sort(rids.begin(), rids.end());
        for (std::size_t ri=0; ri<rids.size(); ++ri){
          if (ri) out << ",";
          out << rids[ri];
        }
        out << "]}";
      }
      out << "]}";
      if (fi+1<ids3.size()) out << ",";
      out << "\n";
    }

    out << "    ]\n";
    out << "  }\n";
  }

  out << "}\n";
}

} // namespace bruce::io
