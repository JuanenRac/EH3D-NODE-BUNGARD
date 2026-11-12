// EH3D-NODE-BUNGARD - the mark of the board that a firmware image carries, and the check of an image that is about to be installed. No hardware.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The images of one project for two boards (the Ethernet one and the Wi-Fi one) have the same project name, so the name alone cannot tell a wrong image from a right one: an Ethernet image put on
// a Wi-Fi board starts, finds no network of the kind it was made for and can leave the node out of reach. Every image therefore carries a text `EH3D-BOARD-MARK:<board>` (main/node_guard.cpp), and
// after the image is written (esp_ota_end) the node reads it back from the flash, a few KB at a time, with the scanner below and compares the board it names with its own.
//   * the mark names another board: the image is refused (the same answer as an image of another project);
//   * the image has no mark (made before the mark existed): accepted, unless `kRequireMark` says otherwise, so that a node can still be brought forward from an old image;
//   * more than one mark with different boards: refused - an image that does not know which board it is for is not installed.
// The scanner works on pieces of any size (a mark that falls across two reads is found) and does not allocate.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#if defined(EH3D_BOARD_S3_ETH)
#define EH3D_BOARD_MARK_TEXT "EH3D-BOARD-MARK:s3-eth"
#else
#define EH3D_BOARD_MARK_TEXT "EH3D-BOARD-MARK:s3-wifi"
#endif

namespace eh3d::boardmark {

constexpr char kPrefix[] = "EH3D-BOARD-MARK:";
constexpr std::size_t kPrefixLength = sizeof kPrefix - 1;
constexpr std::size_t kMaxBoardLength = 24;
// false: an image with no mark is accepted (an image made before the mark was added). Changing it to true refuses every image without one.
constexpr bool kRequireMark = false;

class Scanner {
 public:
  // Takes the next piece of the image, in order.
  void feed(const std::uint8_t* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) step(data[i]);
  }
  void feed(const char* text) { feed(reinterpret_cast<const std::uint8_t*>(text), std::strlen(text)); }
  // Whether a mark with a board name was seen, which board it names, and whether two marks disagreed.
  bool found() const { return !board_.empty(); }
  const std::string& board() const { return board_; }
  bool conflict() const { return conflict_; }

 private:
  void step(std::uint8_t c) {
    if (reading_) {
      if (c >= 0x21 && c <= 0x7E && current_.size() <= kMaxBoardLength) {
        current_.push_back(static_cast<char>(c));
        return;
      }
      finish();   // a character that cannot be in a board name ends the mark (the zero of the text)
      reading_ = false;
      matched_ = 0;
    }
    if (c == static_cast<std::uint8_t>(kPrefix[matched_])) {
      if (++matched_ == kPrefixLength) {
        reading_ = true;
        matched_ = 0;
        current_.clear();
      }
    } else {
      matched_ = c == static_cast<std::uint8_t>(kPrefix[0]) ? 1 : 0;   // the prefix does not repeat its first letter, so one letter is all that can carry over
    }
  }
  void finish() {
    // The prefix as the scanner itself keeps it (followed by the zero of its text) is not a mark; a name that is too long is not a board either.
    if (current_.empty() || current_.size() > kMaxBoardLength) return;
    if (board_.empty()) board_ = current_;
    else if (board_ != current_) conflict_ = true;
  }
  std::size_t matched_ = 0;
  bool reading_ = false;
  std::string current_;
  std::string board_;
  bool conflict_ = false;

 public:
  // The image ended: a mark that was being read when the data stopped counts.
  void end() {
    if (reading_) {
      finish();
      reading_ = false;
    }
  }
};

enum class Verdict { kOk, kNoMark, kWrongBoard };

inline Verdict verdict(const Scanner& scan, const std::string& own_board) {
  if (scan.conflict()) return Verdict::kWrongBoard;
  if (!scan.found()) return Verdict::kNoMark;
  return scan.board() == own_board ? Verdict::kOk : Verdict::kWrongBoard;
}

// Whether the image may be installed.
inline bool accept(Verdict v, bool require_mark = kRequireMark) { return v == Verdict::kOk || (v == Verdict::kNoMark && !require_mark); }

}  // namespace eh3d::boardmark
