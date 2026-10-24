// EH3D-NODE-BUNGARD - the frames of the link between the node and the program that drives the machine.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A frame is: sync 0xB5, type (1 byte), payload length (2 bytes, little endian), the payload, and a CRC-16/CCITT-FALSE
// (2 bytes, little endian) over the type, the length and the payload. The decoder resynchronises on any damage.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace eh3d::bungard {

constexpr std::uint8_t kSync = 0xB5;
constexpr std::size_t kMaxPayload = 2048;
constexpr std::size_t kHeaderBytes = 4;
constexpr std::size_t kMaxFrameBytes = kHeaderBytes + kMaxPayload + 2;

enum class Type : std::uint8_t {
  // program -> node
  Hello = 0x01,  // payload: the access token (text); must be the first frame
  Arm = 0x02,    // the operator allows the machine to be moved
  Disarm = 0x03,
  Dtr = 0x04,    // payload: 1 byte, 0 = DTR off, otherwise on
  Write = 0x05,  // payload: bytes for the control unit, sent in order at the line's own pace
  Abort = 0x06,  // empty the queue, switch the drivers off, disarm
  Ping = 0x07,   // keeps an armed session alive
  StopOnHome = 0x09,  // payload: 1 byte, the axes to watch (bit 0 X, bit 1 Y, bit 2 Z; 0 cancels). When the HOME lamp of one of them lights, the node empties its queue
  Spindle = 0x08,  // payload: speed in rpm (u16 LE), optionally 1 byte direction (0 forward, 1 reverse; left out: the last one) and 1 byte of flags (bit 0: at the end of the queue, not now); 0 rpm stops it
  // node -> program
  Info = 0x81,    // payload: major, minor, patch (3 bytes), baud (u32 LE), queue capacity (u32 LE)
  Status = 0x82,  // payload: queue fill (u32 LE), bytes sent (u32 LE), armed (1), lines (1: CTS 1, DSR 2, RI 4, DCD 8), spindle rpm (u16 LE), spindle direction (1), lamps (1: HOME X 1, HOME Y 2, HOME Z 4, ENABLE 8, TOOL 16), flags (1: bit 0 the node is finishing the job it holds)
  Rx = 0x83,      // payload: bytes the control unit sent
  Error = 0x84,   // payload: 1 byte, see Error below
  HomeSeen = 0x85,  // payload: 1 byte, the lamps (see Status) when a StopOnHome made the node empty its queue
};

enum class Error : std::uint8_t {
  NotAuthenticated = 1,
  BadToken = 2,
  NotArmed = 3,
  QueueFull = 4,
  BadFrame = 5,
  OpenRefused = 6,  // the node has no token and is not set to accept open control
  IdleTimeout = 7,  // an armed session went quiet: the node stopped
  NoSpindle = 8,         // this node has no spindle output
  SpindleDirection = 9,  // the direction cannot change while the spindle turns: stop it first
  SpindleSettling = 10,  // the spindle was stopped too recently to turn the other way
  Draining = 11,         // the node is finishing the job it holds and takes nothing new: wait, or send Abort
};

inline std::uint16_t crc16(const std::uint8_t* data, std::size_t n, std::uint16_t crc = 0xFFFF) {
  for (std::size_t i = 0; i < n; ++i) {
    crc ^= static_cast<std::uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021) : static_cast<std::uint16_t>(crc << 1);
  }
  return crc;
}

struct Frame {
  Type type{};
  std::uint16_t length = 0;
  std::array<std::uint8_t, kMaxPayload> payload{};
};

// Writes a frame into `out`; returns its size, or 0 if the payload is too long or `cap` too small.
inline std::size_t encode(Type type, const std::uint8_t* payload, std::size_t n, std::uint8_t* out, std::size_t cap) {
  if (n > kMaxPayload || cap < kHeaderBytes + n + 2) return 0;
  out[0] = kSync;
  out[1] = static_cast<std::uint8_t>(type);
  out[2] = static_cast<std::uint8_t>(n & 0xFF);
  out[3] = static_cast<std::uint8_t>(n >> 8);
  for (std::size_t i = 0; i < n; ++i) out[kHeaderBytes + i] = payload[i];
  const std::uint16_t crc = crc16(out + 1, 3 + n);
  out[kHeaderBytes + n] = static_cast<std::uint8_t>(crc & 0xFF);
  out[kHeaderBytes + n + 1] = static_cast<std::uint8_t>(crc >> 8);
  return kHeaderBytes + n + 2;
}

// Stream decoder. Bytes are fed in with feed(); poll() returns the next complete, valid frame, or false when more
// bytes are needed. After damage (a bad length or a bad CRC) the bytes already read are examined again from the byte
// after the false start of frame, so a false start never swallows the good frames behind it. After feed() returns true,
// call poll() until it returns false to take any further frames that were already in the buffer.
class Decoder {
 public:
  bool feed(std::uint8_t byte, Frame& frame) {
    if (end_ == kBuffer) compact();
    if (end_ == kBuffer) { reset(); ++damaged_; }  // cannot happen with a valid parse; never overrun
    buf_[end_++] = byte;
    return poll(frame);
  }

  bool poll(Frame& frame) {
    while (start_ < end_) {
      if (step(buf_[start_++], frame)) {
        if (start_ == end_) { start_ = end_ = 0; }
        return true;
      }
    }
    if (state_ == State::Sync) start_ = end_ = 0;
    return false;
  }

  // Frames thrown away because of a bad length or a bad CRC.
  std::uint32_t damaged() const { return damaged_; }

 private:
  enum class State { Sync, Type, LenLo, LenHi, Payload, CrcLo, CrcHi };
  static constexpr std::size_t kBuffer = 2 * kMaxFrameBytes;

  void reset() { start_ = end_ = 0; state_ = State::Sync; }

  void compact() {
    // drop what is already consumed and no longer needed: bytes before the frame being read
    const std::size_t keep_from = state_ == State::Sync ? start_ : frame_start_;
    std::size_t n = 0;
    for (std::size_t i = keep_from; i < end_; ++i) buf_[n++] = buf_[i];
    if (state_ != State::Sync) frame_start_ = 0;
    start_ -= keep_from;
    end_ = n;
  }

  bool step(std::uint8_t byte, Frame& frame) {
    switch (state_) {
      case State::Sync:
        if (byte == kSync) { frame_start_ = start_ - 1; state_ = State::Type; }
        return false;
      case State::Type:
        type_ = byte; crc_in_ = crc16(&byte, 1); state_ = State::LenLo; return false;
      case State::LenLo:
        len_ = byte; crc_in_ = crc16(&byte, 1, crc_in_); state_ = State::LenHi; return false;
      case State::LenHi:
        len_ = static_cast<std::uint16_t>(len_ | (byte << 8));
        crc_in_ = crc16(&byte, 1, crc_in_);
        if (len_ > kMaxPayload) { fail(); return false; }
        got_ = 0;
        state_ = len_ == 0 ? State::CrcLo : State::Payload;
        return false;
      case State::Payload:
        frame.payload[got_++] = byte;
        crc_in_ = crc16(&byte, 1, crc_in_);
        if (got_ == len_) state_ = State::CrcLo;
        return false;
      case State::CrcLo:
        crc_lo_ = byte; state_ = State::CrcHi; return false;
      case State::CrcHi: {
        const std::uint16_t crc = static_cast<std::uint16_t>(crc_lo_ | (byte << 8));
        if (crc != crc_in_) { fail(); return false; }
        state_ = State::Sync;
        frame.type = static_cast<Type>(type_);
        frame.length = len_;
        return true;
      }
    }
    return false;
  }

  // The frame being read is damaged: go back to the byte after its false start and look again.
  void fail() {
    ++damaged_;
    start_ = frame_start_ + 1;
    state_ = State::Sync;
  }

  State state_ = State::Sync;
  std::uint8_t type_ = 0;
  std::uint16_t len_ = 0;
  std::uint16_t got_ = 0;
  std::uint16_t crc_in_ = 0;
  std::uint8_t crc_lo_ = 0;
  std::uint32_t damaged_ = 0;
  std::uint8_t buf_[kBuffer] = {};
  std::size_t start_ = 0;       // next byte to examine
  std::size_t end_ = 0;         // bytes held
  std::size_t frame_start_ = 0; // where the frame being read began
};

}  // namespace eh3d::bungard
