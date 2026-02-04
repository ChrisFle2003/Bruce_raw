#include "bruce/core/ops/ops.hpp"
#include "bruce/core/axes/axis_registry.hpp"
#include <cassert>

int main(){
  bruce::core::OwnedAxes owned;
  owned.minv = { -100, -100 };
  owned.maxv = {  100,  100 };
  owned.max_abs = { 100, 100 };
  owned.labels = { {"negA","posA"}, {"negB","posB"} };
  auto axes = owned.view();

  std::vector<bruce::val_t> a = { -100, 50 };
  std::vector<bruce::val_t> b = { 100, -50 };
  std::vector<bruce::val_t> out(axes.n);
  bruce::core::ops::mix_into(a,b,axes,out);
  assert(out[0] == 0 && out[1] == 0);

  auto d = bruce::core::ops::l1_distance(a,b);
  assert(d == 300);

  auto c = bruce::core::ops::collapse_to_neutral(out, axes);
  assert(c == 1.0);
  return 0;
}
