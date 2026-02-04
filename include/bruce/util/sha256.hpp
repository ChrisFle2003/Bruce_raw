#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bruce::util {

class Sha256 {
public:
  Sha256();
  void update(const std::uint8_t* data, std::size_t len);
  std::array<std::uint8_t, 32> finalize();

private:
  void transform(const std::uint8_t block[64]);

  std::uint32_t state_[8];
  std::uint64_t bitlen_;
  std::uint8_t buffer_[64];
  std::size_t buffer_len_;
  bool finalized_;
};

std::string to_hex(const std::array<std::uint8_t, 32>& digest);

} // namespace bruce::util
