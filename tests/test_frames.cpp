// EH3D-NODE-BUNGARD host tests: the frames of the control link against the shared test vectors (tests/testdata/frames.json), the same file that the Rust side of the link reads.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../core/frame.hpp"
#include "../core/json.hpp"

using namespace eh3d;
using namespace eh3d::bungard;

static int g_checks = 0;
static int g_failed = 0;
#define CHECK(cond)                                                                                  \
  do {                                                                                               \
    ++g_checks;                                                                                      \
    if (!(cond)) { ++g_failed; std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); }       \
  } while (0)

static std::string source(const std::string& rel) {
  for (const char* up : {"", "../", "../../", "../../../"}) {
    std::ifstream in(std::string(up) + rel, std::ios::binary);
    if (!in) continue;
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
  }
  return std::string();
}

static bool hex_to_bytes(const std::string& hex, std::vector<std::uint8_t>& out) {
  out.clear();
  if (hex.size() % 2 != 0) return false;
  auto nibble = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; };
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int hi = nibble(hex[i]), lo = nibble(hex[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out.push_back(static_cast<std::uint8_t>(hi * 16 + lo));
  }
  return true;
}

static std::vector<Frame> decode_all(Decoder& d, const std::vector<std::uint8_t>& bytes) {
  std::vector<Frame> frames;
  Frame f;
  for (std::uint8_t b : bytes) {
    if (d.feed(b, f)) {
      frames.push_back(f);
      while (d.poll(f)) frames.push_back(f);
    }
  }
  return frames;
}

int main() {
  json::Value doc;
  const std::string text = source("tests/testdata/frames.json");
  CHECK(!text.empty());
  CHECK(json::parse(text, doc));
  CHECK(doc.integer_or("sync", 0, 0, 255) == kSync);
  const json::Value* vectors = doc.get("vectors");
  const json::Value* damaged = doc.get("damaged");
  CHECK(vectors != nullptr && vectors->is_array() && vectors->items.size() >= 10);
  CHECK(damaged != nullptr && damaged->is_array() && damaged->items.size() >= 4);
  if (vectors == nullptr || damaged == nullptr) { std::printf("%d checks, %d failures\n", g_checks, g_failed); return 1; }

  // every vector: the frame the node writes is the one of the file, and the decoder gives the type and the payload back
  for (const json::Value& v : vectors->items) {
    std::vector<std::uint8_t> payload, want;
    const long long type = v.integer_or("type", -1, 0, 255);
    CHECK(type >= 0 && hex_to_bytes(v.string_or("payload", "?"), payload) && hex_to_bytes(v.string_or("frame", "?"), want));
    std::vector<std::uint8_t> got(kMaxFrameBytes);
    const std::size_t n = encode(static_cast<Type>(type), payload.data(), payload.size(), got.data(), got.size());
    got.resize(n);
    CHECK(got == want);
    Decoder d;
    const std::vector<Frame> frames = decode_all(d, want);
    CHECK(frames.size() == 1 && d.damaged() == 0);
    if (frames.size() == 1) {
      CHECK(static_cast<long long>(frames[0].type) == type && frames[0].length == payload.size());
      CHECK(std::vector<std::uint8_t>(frames[0].payload.begin(), frames[0].payload.begin() + payload.size()) == payload);
    }
    // fed in one byte at a time or all at once, the result is the same, and a decoder that goes on after the frame takes the next one
    Decoder again;
    std::vector<std::uint8_t> twice = want;
    twice.insert(twice.end(), want.begin(), want.end());
    CHECK(decode_all(again, twice).size() == 2);
  }

  // damaged bytes: how many good frames come out and that the damage is counted
  for (const json::Value& v : damaged->items) {
    std::vector<std::uint8_t> bytes;
    CHECK(hex_to_bytes(v.string_or("bytes", "?"), bytes));
    Decoder d;
    const std::vector<Frame> frames = decode_all(d, bytes);
    CHECK(static_cast<long long>(frames.size()) == v.integer_or("frames", -1, 0, 100));
    CHECK(static_cast<long long>(d.damaged()) >= v.integer_or("damaged_at_least", 1000, 0, 100));
  }

  std::printf("%d checks, %d failures\n", g_checks, g_failed);
  return g_failed == 0 ? 0 : 1;
}
