#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace bruce::io::minijson {

struct Value {
  enum class Type { Null, Bool, Int, String, Array, Object } type = Type::Null;
  bool b = false;
  std::int64_t i = 0;
  std::string s;
  std::vector<Value> a;
  std::unordered_map<std::string, Value> o;

  const Value& at(std::string_view key) const;
  bool is(Type t) const { return type == t; }
};

Value parse(std::string_view json);
Value parse_file(const std::string& path);

} // namespace bruce::io::minijson
