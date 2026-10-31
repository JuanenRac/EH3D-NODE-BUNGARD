// EH3D-NODE-BUNGARD - which file of a GitHub release is the firmware of THIS board, and which is its checksum. Pure: tested on a computer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A release of an EH3D node attaches, for each board, the image that the panel of the node can install, `<image>-<version>-<board>-app.bin` (for example
// `eh3d-node-bungard-0.0.3-s3-eth-app.bin`), the SHA-256 of each one (`<name>.sha256`) and the complete image that is written by cable (`...-complete.bin`, never installed from
// here). A node finds the image built for its own board and for its own project, and never installs one that is not its own.
#pragma once
#include <string>

#include "json.hpp"

namespace eh3d::release {

// Only what GitHub itself publishes for this repository is downloaded and installed: https://github.com/<owner>/<repo>/releases/download/<tag>/<file>, with nothing but one tag and one
// file name after it. (An address that comes out of the release's JSON is still checked: a node that asks for any address it is handed can be made to install from anywhere.)
inline bool is_official_url(const std::string& url, const std::string& repo) {
  const std::string prefix = "https://github.com/" + repo + "/releases/download/";
  if (repo.empty() || url.compare(0, prefix.size(), prefix) != 0) return false;
  std::size_t slashes = 0;
  for (std::size_t i = prefix.size(); i < url.size(); ++i) {
    const char c = url[i];
    if (c == '/') ++slashes;
    else if (c == '?' || c == '#' || c == '\\' || c == '@' || c <= ' ' || c == '%') return false;
  }
  return slashes == 1 && url.size() > prefix.size() + 3 && url.find("..") == std::string::npos && url.back() != '/';
}

// The buffer the HTTP client keeps for the headers of an answer. GitHub redirects a download to a storage address whose headers add up to more than 5 KB (a signed Location of about a
// thousand characters, a security policy, cookies): a buffer of 4 KB made the client give up with "out of buffer". An allocation of 4096 bytes or less goes to INTERNAL memory (there is
// only some 65 to 80 KB of it free) and a bigger one to the external RAM first, so 20480 bytes is both roomy and free of cost for the internal heap. A board with no external RAM
// (or none free) takes the smallest buffer that holds those headers.
constexpr int kHeaderBufferExternal = 20480;
constexpr int kHeaderBufferInternal = 6144;
// The REQUEST buffer of the HTTP client (its default is 512 bytes): after GitHub's redirect the request line alone carries an address of about a thousand characters.
constexpr int kRequestBuffer = 4096;
static_assert(kRequestBuffer >= 2048, "the request buffer must hold GitHub's long redirect address");
inline int header_buffer_size(std::size_t free_external_bytes) { return free_external_bytes >= 2u * static_cast<std::size_t>(kHeaderBufferExternal) ? kHeaderBufferExternal : kHeaderBufferInternal; }

// What to tell when the checksum of a release could not be read: the real reason, never "the release has none" (that is only for a release that does not carry the file). `problem` is
// what the download said: connect_failed, read_error, client_init, http_<status> or too_large.
inline std::string fetch_error(const std::string& problem) {
  if (problem.compare(0, 5, "http_") == 0) return problem;
  if (problem == "too_large") return "checksum_too_large";
  return "checksum_unreachable";
}

struct Picked {
  std::string image_url;      // empty: the release has no image for this board
  std::string checksum_url;   // empty: it has the image but not the checksum file next to it
};

inline bool starts_with(const std::string& text, const std::string& prefix) { return text.compare(0, prefix.size(), prefix) == 0; }
inline bool ends_with(const std::string& text, const std::string& suffix) { return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0; }

// `assets` is the "assets" array of the release's JSON (each with "name" and "browser_download_url"). The image is `<image>-<version>-<board>-app.bin`: it starts with the project's image name and a
// dash, and ends with a dash, the board and `-app.bin`. The project `eh3d-node-bungard` must not take the image of `eh3d-node-bungard-aux` (whose name also starts with it): the version, which
// begins with a digit, comes right after the image name.
inline Picked pick(const json::Value* assets, const char* image, const char* board_id) {
  Picked picked;
  if (assets == nullptr || !assets->is_array() || image == nullptr || board_id == nullptr) return picked;
  const std::string prefix = std::string(image) + "-", suffix = std::string("-") + board_id + "-app.bin";
  std::string name_found;
  for (const json::Value& asset : assets->items) {
    const std::string name = asset.string_or("name", ""), url = asset.string_or("browser_download_url", "");
    if (!starts_with(name, prefix) || !ends_with(name, suffix) || name.size() <= prefix.size() + suffix.size()) continue;
    const char first_of_version = name[prefix.size()];
    if (first_of_version < '0' || first_of_version > '9') continue;
    picked.image_url = url;
    name_found = name;
  }
  if (name_found.empty()) return picked;
  for (const json::Value& asset : assets->items) {
    if (asset.string_or("name", "") == name_found + ".sha256") picked.checksum_url = asset.string_or("browser_download_url", "");
  }
  return picked;
}

}  // namespace eh3d::release
