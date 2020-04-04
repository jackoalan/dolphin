// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Abstraction for atomically passing 2D coordinates.
// The return value of Fetch can be used as a flag to indicate window resizes while also providing
// the latest dimensions.

#pragma once

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

#include <atomic>

namespace Common
{
class AtomicInt2D final
{
  union Bits
  {
    BitField<0, 32, u64> m_width;
    BitField<32, 32, u64> m_height;
    u64 m_hex = UINT64_MAX;
  };

public:
  AtomicInt2D() = default;

  AtomicInt2D(const AtomicInt2D& other)
      : m_val(other.m_val.load()), m_fetched_width(other.m_fetched_width),
        m_fetched_height(other.m_fetched_height)
  {
  }

  void Store(int width, int height)
  {
    Bits b;
    b.m_width = static_cast<u32>(width);
    b.m_height = static_cast<u32>(height);
    m_val.store(b.m_hex);
  }

  bool Fetch(int& width_out, int& height_out)
  {
    Bits b;
    if (m_val.compare_exchange_strong(b.m_hex, UINT64_MAX))
    {
      width_out = m_fetched_width;
      height_out = m_fetched_height;
      return false;
    }
    width_out = m_fetched_width = static_cast<s32>(b.m_width);
    height_out = m_fetched_height = static_cast<s32>(b.m_height);
    return true;
  }

private:
  std::atomic_uint64_t m_val{UINT64_MAX};
  int m_fetched_width = 0, m_fetched_height = 0;
};

}  // namespace Common
