// EH3D-NODE-BUNGARD - checking GitHub's own releases for a newer firmware and installing it, next to the manual upload of the panel (web_server.cpp's /api/v1/ota): both end up in the same
// OTA slot through the same esp_ota_* calls, this one just gets its bytes from the Internet instead of the panel's own upload. Nothing here replaces the manual path - a board with no
// Internet route, or a fleet an operator wants to hold back, still updates exactly as it always has.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <string>

namespace eh3d::github_update {

struct CheckResult {
  bool ok = false;             // false: `error` says why (network, no release, no matching asset...)
  std::string error;
  std::string latest_version;  // the release's own tag, without a leading "v"
  std::string asset_url;       // empty when the latest release has no image for this board
  std::string sha256;          // the hash the release publishes for it (64 lowercase hex digits); empty when it publishes none
  bool update_available = false;
};

// Asks GitHub's API for the newest release of this project's repository (`repo`, "Owner/Name") and compares it to the running firmware; `image` is the name the images of this project start
// with (`eh3d-node-bungard`) and `board_id` says which image is this node's. Blocks for the duration of the HTTPS request.
CheckResult check(const char* repo, const char* image, const char* board_id);

struct InstallResult {
  bool ok = false;
  std::string error;
  std::string version;
  std::size_t bytes = 0;
};

// The install runs in a task of its own so the panel can show how far it is: start_latest() returns at once, progress() says where it stands. `state` is "idle", "downloading",
// "verifying", "done" or "failed" (with `error`); a "done" install has already set the new image to boot and the caller restarts.
struct Progress {
  std::string state = "idle";
  std::size_t got = 0;
  std::size_t total = 0;
  std::string error;
  std::string version;
};
Progress progress();

// Copies the firmware that runs into the other slot (ota_0 <-> ota_1), so both hold the same version: a copy to fall back to. It runs in a task of its own like an install; the running slot is
// only read, the other one is erased and written, the image is checked at the end and nothing is set to boot (switching slots is a separate step).
struct CopyProgress {
  std::string state = "idle";   // "idle", "copying", "done" or "failed"
  std::size_t done = 0;
  std::size_t total = 0;
  std::string error;
  std::string version;
};
bool start_copy();   // false when an install, a look or a copy is already running
CopyProgress copy_progress();

// The look for a newer release (and the install of it) in a task of their own: the web server has few threads, and a request to GitHub over TLS takes seconds. `start_check` returns at
// once; `check_progress` says "idle", "checking", "done" or "failed" and, when it is done, what was found. `start_latest` checks and then installs what it found, all in the task.
struct CheckProgress {
  std::string state = "idle";
  CheckResult result;
};
bool start_check(const char* repo, const char* image, const char* board_id);
CheckProgress check_progress();
bool start_latest(const char* repo, const char* image, const char* board_id);

// The manual upload of the panel writes the same slot as an installation from GitHub and a copy of the slot: only one of them at a time. `begin_manual_write` is false while another is
// running; the caller calls `end_manual_write` when it is done.
bool begin_manual_write();
void end_manual_write();

// Downloads what check() offered (and only that: the address must be one of the repository's own release files) straight into the next OTA slot - the same checks as an upload (minimum
// size, the image's own magic byte, the project name) plus three more: the image must hash to the hash the release publishes; the version inside it must be the release's own and newer than
// the one that runs (an old image cannot be installed again). Only then is it set to boot. Does not restart; the caller decides when. One install at a time ("ota_busy").
// The releases are not signed yet: what protects the install is the HTTPS connection to github.com and the hash the release publishes next to the image.
InstallResult install(const char* repo, const CheckResult& offer);

}  // namespace eh3d::github_update
