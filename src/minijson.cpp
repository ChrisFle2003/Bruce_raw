#include "bruce/io/minijson.hpp"
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cctype>

namespace bruce::io::minijson {

static void fail(const char* msg){ throw std::runtime_error(msg); }

struct P {
  std::string_view s; std::size_t p=0;
  char peek() const { return p < s.size() ? s[p] : '\0'; }
  char get(){ return p < s.size() ? s[p++] : '\0'; }
  void ws(){ while(p<s.size()){ char c=s[p]; if(c==' '||c=='\n'||c=='\r'||c=='\t') p++; else break; } }
  bool consume(char c){ ws(); if(peek()==c){ p++; return true;} return false; }
  void expect(char c){ ws(); if(get()!=c) fail("minijson: expected char"); }

  std::string parse_string(){
    ws(); if(get()!='\"') fail("minijson: expected string");
    std::string out;
    while(true){
      if(p>=s.size()) fail("minijson: unterminated string");
      char c=get();
      if(c=='\"') break;
      if(c=='\\'){
        if(p>=s.size()) fail("minijson: bad escape");
        char e=get();
        switch(e){
          case '\"': out.push_back('\"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          default: fail("minijson: unsupported escape");
        }
      } else out.push_back(c);
    }
    return out;
  }

  std::int64_t parse_int(){
    ws(); bool neg=false;
    if(peek()=='-'){ neg=true; get(); }
    if(!std::isdigit((unsigned char)peek())) fail("minijson: expected int");
    std::int64_t v=0;
    while(std::isdigit((unsigned char)peek())) v = v*10 + (get()-'0');
    return neg ? -v : v;
  }

  Value parse_value(){
    ws();
    char c=peek();
    if(c=='{') return parse_object();
    if(c=='[') return parse_array();
    if(c=='\"'){ Value v; v.type=Value::Type::String; v.s=parse_string(); return v; }
    if(c=='-'||std::isdigit((unsigned char)c)){ Value v; v.type=Value::Type::Int; v.i=parse_int(); return v; }
    if(s.substr(p,4)=="true"){ p+=4; Value v; v.type=Value::Type::Bool; v.b=true; return v; }
    if(s.substr(p,5)=="false"){ p+=5; Value v; v.type=Value::Type::Bool; v.b=false; return v; }
    if(s.substr(p,4)=="null"){ p+=4; Value v; v.type=Value::Type::Null; return v; }
    fail("minijson: unexpected token");
    return {};
  }

  Value parse_array(){
    Value v; v.type=Value::Type::Array;
    expect('['); ws();
    if(consume(']')) return v;
    while(true){
      v.a.push_back(parse_value());
      ws();
      if(consume(']')) break;
      expect(',');
    }
    return v;
  }

  Value parse_object(){
    Value v; v.type=Value::Type::Object;
    expect('{'); ws();
    if(consume('}')) return v;
    while(true){
      std::string k=parse_string();
      expect(':');
      Value val=parse_value();
      v.o.emplace(std::move(k), std::move(val));
      ws();
      if(consume('}')) break;
      expect(',');
    }
    return v;
  }
};

const Value& Value::at(std::string_view key) const {
  auto it = o.find(std::string(key));
  if(it==o.end()) throw std::runtime_error("minijson: missing key");
  return it->second;
}

Value parse(std::string_view json){
  P p{json,0};
  Value v = p.parse_value();
  p.ws();
  if(p.p != json.size()) throw std::runtime_error("minijson: trailing garbage");
  return v;
}

Value parse_file(const std::string& path){
  std::ifstream in(path, std::ios::binary);
  if(!in) throw std::runtime_error("minijson: cannot open file");
  std::ostringstream ss; ss<<in.rdbuf();
  return parse(ss.str());
}

} // namespace bruce::io::minijson
