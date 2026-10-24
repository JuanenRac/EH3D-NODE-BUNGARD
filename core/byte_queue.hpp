// EH3D-NODE-BUNGARD - the queue of bytes waiting to go out to the control unit.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A fixed ring. push() is all-or-nothing, so a frame's bytes are never half accepted. Not thread safe: the
// firmware guards it with a mutex.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eh3d::bungard {

class ByteQueue {
 public:
  explicit ByteQueue(std::size_t capacity) : buf_(capacity) {}

  std::size_t capacity() const { return buf_.size(); }
  std::size_t size() const { return size_; }
  std::size_t free() const { return buf_.size() - size_; }

  bool push(const std::uint8_t* data, std::size_t n) {
    if (n > free()) return false;
    for (std::size_t i = 0; i < n; ++i) {
      buf_[(head_ + size_) % buf_.size()] = data[i];
      ++size_;
    }
    return true;
  }

  std::size_t pop(std::uint8_t* out, std::size_t max) {
    std::size_t n = 0;
    while (n < max && size_ > 0) {
      out[n++] = buf_[head_];
      head_ = (head_ + 1) % buf_.size();
      --size_;
    }
    return n;
  }

  void clear() { head_ = 0; size_ = 0; }

 private:
  std::vector<std::uint8_t> buf_;
  std::size_t head_ = 0;
  std::size_t size_ = 0;
};

}  // namespace eh3d::bungard
