#include "bruce/util/sha256.hpp"
#include <cstring>

namespace bruce::util {

static inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32u - n));
}

static inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  return (x & y) ^ (~x & z);
}

static inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

static inline std::uint32_t big_sigma0(std::uint32_t x) {
  return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static inline std::uint32_t big_sigma1(std::uint32_t x) {
  return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static inline std::uint32_t small_sigma0(std::uint32_t x) {
  return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static inline std::uint32_t small_sigma1(std::uint32_t x) {
  return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

static const std::uint32_t k[64] = {
  0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
  0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
  0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
  0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
  0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
  0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
  0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
  0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
  0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
  0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
  0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
  0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
  0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
  0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
  0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
  0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

Sha256::Sha256()
  : state_{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
           0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u},
    bitlen_(0),
    buffer_{0},
    buffer_len_(0),
    finalized_(false) {}

void Sha256::update(const std::uint8_t* data, std::size_t len) {
  if (finalized_) return;
  for (std::size_t i = 0; i < len; ++i) {
    buffer_[buffer_len_++] = data[i];
    if (buffer_len_ == 64) {
      transform(buffer_);
      bitlen_ += 512;
      buffer_len_ = 0;
    }
  }
}

std::array<std::uint8_t, 32> Sha256::finalize() {
  if (finalized_) {
    std::array<std::uint8_t, 32> out{};
    for (int i = 0; i < 8; ++i) {
      out[i * 4 + 0] = (state_[i] >> 24) & 0xff;
      out[i * 4 + 1] = (state_[i] >> 16) & 0xff;
      out[i * 4 + 2] = (state_[i] >> 8) & 0xff;
      out[i * 4 + 3] = (state_[i]) & 0xff;
    }
    return out;
  }

  std::uint64_t total_bits = bitlen_ + (buffer_len_ * 8u);
  buffer_[buffer_len_++] = 0x80u;
  if (buffer_len_ > 56) {
    while (buffer_len_ < 64) buffer_[buffer_len_++] = 0;
    transform(buffer_);
    buffer_len_ = 0;
  }
  while (buffer_len_ < 56) buffer_[buffer_len_++] = 0;

  for (int i = 7; i >= 0; --i) {
    buffer_[buffer_len_++] = static_cast<std::uint8_t>((total_bits >> (i * 8)) & 0xff);
  }

  transform(buffer_);
  finalized_ = true;

  std::array<std::uint8_t, 32> out{};
  for (int i = 0; i < 8; ++i) {
    out[i * 4 + 0] = (state_[i] >> 24) & 0xff;
    out[i * 4 + 1] = (state_[i] >> 16) & 0xff;
    out[i * 4 + 2] = (state_[i] >> 8) & 0xff;
    out[i * 4 + 3] = (state_[i]) & 0xff;
  }
  return out;
}

void Sha256::transform(const std::uint8_t block[64]) {
  std::uint32_t m[64];
  for (int i = 0; i < 16; ++i) {
    std::size_t j = static_cast<std::size_t>(i) * 4;
    m[i] = (static_cast<std::uint32_t>(block[j]) << 24) |
           (static_cast<std::uint32_t>(block[j + 1]) << 16) |
           (static_cast<std::uint32_t>(block[j + 2]) << 8) |
           (static_cast<std::uint32_t>(block[j + 3]));
  }
  for (int i = 16; i < 64; ++i) {
    m[i] = small_sigma1(m[i - 2]) + m[i - 7] + small_sigma0(m[i - 15]) + m[i - 16];
  }

  std::uint32_t a = state_[0];
  std::uint32_t b = state_[1];
  std::uint32_t c = state_[2];
  std::uint32_t d = state_[3];
  std::uint32_t e = state_[4];
  std::uint32_t f = state_[5];
  std::uint32_t g = state_[6];
  std::uint32_t h = state_[7];

  for (int i = 0; i < 64; ++i) {
    std::uint32_t t1 = h + big_sigma1(e) + ch(e, f, g) + k[i] + m[i];
    std::uint32_t t2 = big_sigma0(a) + maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
  state_[5] += f;
  state_[6] += g;
  state_[7] += h;
}

std::string to_hex(const std::array<std::uint8_t, 32>& digest) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (std::uint8_t b : digest) {
    out.push_back(hex[(b >> 4) & 0x0f]);
    out.push_back(hex[b & 0x0f]);
  }
  return out;
}

} // namespace bruce::util
