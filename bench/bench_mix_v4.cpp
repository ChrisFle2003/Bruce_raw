#include "bruce/core/ops/ops.hpp"
#include "bruce/core/axes/axis_registry.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>

// -------------------------
// Args
// -------------------------
struct Args {
  std::string test = "mix";   // mix | allowed | nearest | e2e | stress
  std::string mode = "best";  // best | worst (nur allowed)
  int loops = 200000;
  int runs = 1;
  int warmup = 0;
  int dims = 7;
  int N = 10000;              // nearest/e2e corpus size
  int stress_mix = 2000;
  int stress_allowed = 2000;
  int stress_nearest = 50;
};

static std::string need(int& i, int argc, char** argv, const char* name){
  if (i + 1 >= argc) { std::cerr << "Missing value for " << name << "\n"; std::exit(2); }
  return argv[++i];
}

static Args parse_args(int argc, char** argv){
  Args a;
  for (int i = 1; i < argc; ++i){
    std::string k = argv[i];
    if (k == "--test")   a.test = need(i, argc, argv, "--test");
    else if (k == "--mode")   a.mode = need(i, argc, argv, "--mode");
    else if (k == "--loops")  a.loops = std::stoi(need(i, argc, argv, "--loops"));
    else if (k == "--runs")   a.runs  = std::stoi(need(i, argc, argv, "--runs"));
    else if (k == "--warmup") a.warmup= std::stoi(need(i, argc, argv, "--warmup"));
    else if (k == "--dims")   a.dims  = std::stoi(need(i, argc, argv, "--dims"));
    else if (k == "--N")      a.N     = std::stoi(need(i, argc, argv, "--N"));
    else if (k == "--stress-mix") a.stress_mix = std::stoi(need(i, argc, argv, "--stress-mix"));
    else if (k == "--stress-allowed") a.stress_allowed = std::stoi(need(i, argc, argv, "--stress-allowed"));
    else if (k == "--stress-nearest") a.stress_nearest = std::stoi(need(i, argc, argv, "--stress-nearest"));
    // ignoriert unbekannte args absichtlich (damit dein altes CLI nicht bricht)
  }
  if (a.dims <= 0) a.dims = 1;
  if (a.loops <= 0) a.loops = 1;
  if (a.N <= 0) a.N = 1;
  if (a.runs <= 0) a.runs = 1;
  if (a.warmup < 0) a.warmup = 0;
  if (a.stress_mix < 0) a.stress_mix = 0;
  if (a.stress_allowed < 0) a.stress_allowed = 0;
  if (a.stress_nearest < 0) a.stress_nearest = 0;
  return a;
}

// -------------------------
// Deterministic generator
// -------------------------
static inline uint32_t xorshift32(uint32_t& s){
  s ^= s << 13; s ^= s >> 17; s ^= s << 5;
  return s;
}

static std::vector<bruce::val_t> make_vec(int dims, uint32_t seed){
  std::vector<bruce::val_t> v((size_t)dims);
  uint32_t s = seed;
  for (int i=0;i<dims;i++){
    int r = (int)(xorshift32(s) % 20001) - 10000;
    v[(size_t)i] = (bruce::val_t)r;
  }
  return v;
}

static inline bruce::val_t vabs(bruce::val_t x){ return x < 0 ? -x : x; }

static inline std::int64_t l1(const bruce::val_t* a, const bruce::val_t* b, int dims){
  std::int64_t sum = 0;
  for (int i=0;i<dims;i++) sum += (std::int64_t)vabs(a[i] - b[i]);
  return sum;
}

// -------------------------
// Bench tests
// -------------------------
static std::int64_t bench_mix(int loops, int dims){
  bruce::core::OwnedAxes owned;
  owned.minv.assign((size_t)dims, -10000);
  owned.maxv.assign((size_t)dims,  10000);
  owned.max_abs.assign((size_t)dims, 10000);
  owned.labels.assign((size_t)dims, {"n","p"});
  auto axes = owned.view();

  auto a = make_vec(dims, 1);
  auto b = make_vec(dims, 2);
  std::vector<bruce::val_t> out((size_t)dims);

  auto t0 = std::chrono::high_resolution_clock::now();
  for(int i=0;i<loops;++i) bruce::core::ops::mix_into(a,b,axes,out);
  auto t1 = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
}

static inline bool allowed_max_delta(const bruce::val_t* from, const bruce::val_t* to,
                                    const bruce::val_t* maxd, int dims){
  for (int i=0;i<dims;i++){
    if (vabs(to[i] - from[i]) > maxd[i]) return false;
  }
  return true;
}

static std::int64_t bench_allowed(int loops, int dims, const std::string& mode){
  auto from = make_vec(dims, 11);
  auto to   = make_vec(dims, 12);

  std::vector<bruce::val_t> maxd((size_t)dims);
  for (int i=0;i<dims;i++) maxd[(size_t)i] = (bruce::val_t)1500;

  if (mode == "worst"){
    // fail sofort
    to[0] = from[0] + maxd[0] + 1;
  } else {
    // meistens true
    for (int i=0;i<dims;i++) to[(size_t)i] = from[(size_t)i] + (maxd[(size_t)i] / 2);
  }

  volatile int sink = 0;
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int i=0;i<loops;i++){
    sink ^= (int)allowed_max_delta(from.data(), to.data(), maxd.data(), dims);
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  (void)sink;
  return std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
}

static std::int64_t bench_nearest(int loops, int dims, int N){
  // states contiguous
  std::vector<bruce::val_t> states((size_t)N * (size_t)dims);
  uint32_t s = 123;
  for (int j=0;j<N;j++){
    for (int i=0;i<dims;i++){
      int r = (int)(xorshift32(s) % 20001) - 10000;
      states[(size_t)j*(size_t)dims + (size_t)i] = (bruce::val_t)r;
    }
  }
  auto q = make_vec(dims, 999);

  auto one = [&](){
    int best = 0;
    std::int64_t bestd = (std::int64_t)9e18;
    for (int j=0;j<N;j++){
      const bruce::val_t* v = &states[(size_t)j*(size_t)dims];
      std::int64_t d = l1(q.data(), v, dims);
      if (d < bestd){ bestd = d; best = j; }
    }
    return best;
  };

  volatile int sink = 0;
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int i=0;i<loops;i++){
    sink ^= one();
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  (void)sink;
  return std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
}

static std::int64_t bench_e2e(int loops, int dims, int N){
  // tiny deterministic lexicon, IO-free
  std::unordered_map<std::string, std::vector<bruce::val_t>> lex;
  lex["do"]    = make_vec(dims, 1001);
  lex["think"] = make_vec(dims, 1002);
  lex["live"]  = make_vec(dims, 1003);

  // render corpus
  std::vector<bruce::val_t> states((size_t)N * (size_t)dims);
  uint32_t s = 777;
  for (int j=0;j<N;j++){
    for (int i=0;i<dims;i++){
      int r = (int)(xorshift32(s) % 20001) - 10000;
      states[(size_t)j*(size_t)dims + (size_t)i] = (bruce::val_t)r;
    }
  }

  auto tokenize = [](const std::string& in){
    std::vector<std::string> t;
    std::string cur;
    for (char c: in){
      if (c==' ' || c=='\t' || c=='\n'){
        if (!cur.empty()){ t.push_back(cur); cur.clear(); }
      } else {
        if (c>='A' && c<='Z') c = char(c - 'A' + 'a');
        cur.push_back(c);
      }
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
  };

  auto nearest = [&](const std::vector<bruce::val_t>& q){
    int best = 0;
    std::int64_t bestd = (std::int64_t)9e18;
    for (int j=0;j<N;j++){
      const bruce::val_t* v = &states[(size_t)j*(size_t)dims];
      std::int64_t d = l1(q.data(), v, dims);
      if (d < bestd){ bestd = d; best = j; }
    }
    return best;
  };

  auto explain_axes = [&](const std::vector<bruce::val_t>& v){
    // numeric-only summary
    int strong=0, mid=0, light=0;
    for (int i=0;i<dims;i++){
      auto av = vabs(v[(size_t)i]);
      if (av >= 7000) strong++;
      else if (av >= 3000) mid++;
      else if (av >= 1000) light++;
    }
    return strong + mid + light;
  };

  // stable token list
  const std::string input = "do think live";
  auto toks = tokenize(input);

  // axes for mix_into
  bruce::core::OwnedAxes owned;
  owned.minv.assign((size_t)dims, -10000);
  owned.maxv.assign((size_t)dims,  10000);
  owned.max_abs.assign((size_t)dims, 10000);
  owned.labels.assign((size_t)dims, {"n","p"});
  auto axes = owned.view();

  std::vector<bruce::val_t> acc((size_t)dims, 0), tmp((size_t)dims, 0);

  volatile int sink = 0;
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int k=0;k<loops;k++){
    bool used = false;

    for (auto& tk : toks){
      auto it = lex.find(tk);
      if (it == lex.end()) continue;
      if (!used) {
        acc = it->second;
        used = true;
      } else {
        bruce::core::ops::mix_into(acc, it->second, axes, tmp);
        acc.swap(tmp);
      }
    }

    int best = nearest(acc);
    sink ^= best;
    sink ^= explain_axes(acc);
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  (void)sink;
  return std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
}

static std::int64_t bench_stress(const Args& a){
  bruce::core::OwnedAxes owned;
  owned.minv.assign((size_t)a.dims, -10000);
  owned.maxv.assign((size_t)a.dims,  10000);
  owned.max_abs.assign((size_t)a.dims, 10000);
  owned.labels.assign((size_t)a.dims, {"n","p"});
  auto axes = owned.view();

  auto mix_a = make_vec(a.dims, 101);
  auto mix_b = make_vec(a.dims, 202);
  std::vector<bruce::val_t> mix_out((size_t)a.dims);

  auto from = make_vec(a.dims, 303);
  auto to = make_vec(a.dims, 404);
  std::vector<bruce::val_t> maxd((size_t)a.dims);
  for (int i=0;i<a.dims;i++) maxd[(size_t)i] = (bruce::val_t)1500;

  std::vector<bruce::val_t> states((size_t)a.N * (size_t)a.dims);
  uint32_t s = 7777;
  for (int j=0;j<a.N;j++){
    for (int i=0;i<a.dims;i++){
      int r = (int)(xorshift32(s) % 20001) - 10000;
      states[(size_t)j*(size_t)a.dims + (size_t)i] = (bruce::val_t)r;
    }
  }

  auto nearest_once = [&](const std::vector<bruce::val_t>& q){
    int best = 0;
    std::int64_t bestd = (std::int64_t)9e18;
    for (int j=0;j<a.N;j++){
      const bruce::val_t* v = &states[(size_t)j*(size_t)a.dims];
      std::int64_t d = l1(q.data(), v, a.dims);
      if (d < bestd){ bestd = d; best = j; }
    }
    return best;
  };

  volatile int sink = 0;
  auto t0 = std::chrono::high_resolution_clock::now();
  for (int loop=0; loop<a.loops; ++loop){
    for (int k=0; k<a.stress_mix; ++k){
      bruce::core::ops::mix_into(mix_a, mix_b, axes, mix_out);
      mix_a.swap(mix_out);
      mix_b[(size_t)(k % a.dims)] = (bruce::val_t)((int)mix_b[(size_t)(k % a.dims)] + (k & 7) - 3);
    }

    for (int k=0; k<a.stress_allowed; ++k){
      int idx = k % a.dims;
      to[(size_t)idx] = from[(size_t)idx] + (bruce::val_t)((k & 1) ? maxd[(size_t)idx] : (maxd[(size_t)idx] / 2));
      sink ^= (int)allowed_max_delta(from.data(), to.data(), maxd.data(), a.dims);
    }

    uint32_t qseed = 9001u + (uint32_t)loop;
    for (int k=0; k<a.stress_nearest; ++k){
      std::vector<bruce::val_t> q((size_t)a.dims);
      for (int i=0;i<a.dims;i++){
        int r = (int)(xorshift32(qseed) % 20001) - 10000;
        q[(size_t)i] = (bruce::val_t)r;
      }
      sink ^= nearest_once(q);
    }
  }
  auto t1 = std::chrono::high_resolution_clock::now();
  (void)sink;
  return std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
}

// -------------------------
// Main (with warmup + runs median)
// -------------------------
int main(int argc, char** argv){
  Args a = parse_args(argc, argv);

  auto run_once = [&]()->std::int64_t{
    if (a.test == "mix") return bench_mix(a.loops, a.dims);
    if (a.test == "allowed") return bench_allowed(a.loops, a.dims, a.mode);
    if (a.test == "nearest") return bench_nearest(a.loops, a.dims, a.N);
    if (a.test == "e2e") return bench_e2e(a.loops, a.dims, a.N);
    if (a.test == "stress") return bench_stress(a);

    // Default: keep legacy behavior
    return bench_mix(a.loops, a.dims);
  };

  // warmup
  for (int i=0;i<a.warmup;i++) (void)run_once();

  // collect
  std::vector<std::int64_t> samples;
  samples.reserve((size_t)a.runs);
  for (int i=0;i<a.runs;i++) samples.push_back(run_once());
  std::sort(samples.begin(), samples.end());
  auto median = samples[(size_t)samples.size()/2];

  // output (keep your format style)
  std::cout << a.test
            << " loops=" << a.loops
            << " dims=" << a.dims;
  if (a.test == "allowed") std::cout << " mode=" << a.mode;
  if (a.test == "nearest" || a.test == "e2e") std::cout << " N=" << a.N;
  if (a.test == "stress") {
    std::cout << " N=" << a.N
              << " stress_mix=" << a.stress_mix
              << " stress_allowed=" << a.stress_allowed
              << " stress_nearest=" << a.stress_nearest;
  }
  if (a.runs > 1) std::cout << " runs=" << a.runs << " warmup=" << a.warmup;

  std::cout << " : " << median << " us\n";
  return 0;
}
