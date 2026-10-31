// EH3D-NODE-BUNGARD - host tests of the update from GitHub: which file of a release is this board's image, which addresses may be downloaded from, and which versions are newer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <string>

#include "../core/json.hpp"
#include "../core/release_assets.hpp"
#include "../core/semver.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                \
  do {                                                                  \
    ++checks;                                                           \
    if (!(condition)) {                                                 \
      ++failures;                                                       \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);  \
    }                                                                   \
  } while (0)

using namespace eh3d;

static const char* kRepo = "JuanenRac/EH3D-NODE-BUNGARD";
static const std::string kBase = "https://github.com/JuanenRac/EH3D-NODE-BUNGARD/releases/download/v0.0.4/";

static std::string asset(const std::string& name) { return "{\"name\":\"" + name + "\",\"browser_download_url\":\"" + kBase + name + "\"}"; }

static void official_addresses() {
  CHECK(release::is_official_url(kBase + "eh3d-node-bungard-0.0.4-s3-eth-app.bin", kRepo));
  CHECK(!release::is_official_url("https://github.com/Someone/EH3D-NODE-BUNGARD/releases/download/v1/x.bin", kRepo));
  CHECK(!release::is_official_url("http://github.com/JuanenRac/EH3D-NODE-BUNGARD/releases/download/v0.0.4/x.bin", kRepo));
  CHECK(!release::is_official_url(kBase + "a/b.bin", kRepo));           // one tag and one file, nothing deeper
  CHECK(!release::is_official_url(kBase + "x.bin?evil=1", kRepo));
  CHECK(!release::is_official_url(kBase + "..%2f.bin", kRepo));
  CHECK(!release::is_official_url(kBase + "x@y.bin", kRepo));
  CHECK(!release::is_official_url(kBase, kRepo));
  CHECK(!release::is_official_url(kBase + "x.bin", ""));
}

static void picking_the_image() {
  // a release with both boards, the complete image, the checksums and the image of another node of the family
  const std::string doc = "{\"assets\":[" + asset("eh3d-node-bungard-0.0.4-s3-eth-app.bin") + "," + asset("eh3d-node-bungard-0.0.4-s3-eth-app.bin.sha256") + "," + asset("eh3d-node-bungard-0.0.4-s3-eth-complete.bin") + "," +
                          asset("eh3d-node-bungard-0.0.4-s3-wifi-app.bin") + "," + asset("eh3d-node-bungard-0.0.4-s3-wifi-app.bin.sha256") + "," + asset("eh3d-node-bungard-aux-0.0.1-s3-eth-app.bin") + "," +
                          asset("eh3d-node-bungard-aux-0.0.1-s3-eth-app.bin.sha256") + "]}";
  json::Value release;
  CHECK(json::parse(doc, release));
  const json::Value* assets = release.get("assets");
  release::Picked eth = release::pick(assets, "eh3d-node-bungard", "s3-eth");
  CHECK(eth.image_url == kBase + "eh3d-node-bungard-0.0.4-s3-eth-app.bin" && eth.checksum_url == kBase + "eh3d-node-bungard-0.0.4-s3-eth-app.bin.sha256");
  release::Picked wifi = release::pick(assets, "eh3d-node-bungard", "s3-wifi");
  CHECK(wifi.image_url == kBase + "eh3d-node-bungard-0.0.4-s3-wifi-app.bin");
  // the node of the drill head takes its own image and never the one of the machine node
  release::Picked aux = release::pick(assets, "eh3d-node-bungard-aux", "s3-eth");
  CHECK(aux.image_url == kBase + "eh3d-node-bungard-aux-0.0.1-s3-eth-app.bin" && aux.checksum_url == kBase + "eh3d-node-bungard-aux-0.0.1-s3-eth-app.bin.sha256");
  // a board that has no image in the release, or no assets at all
  CHECK(release::pick(assets, "eh3d-node-bungard", "p4").image_url.empty());
  CHECK(release::pick(nullptr, "eh3d-node-bungard", "s3-eth").image_url.empty());
  json::Value empty;
  CHECK(json::parse("{\"assets\":[]}", empty) && release::pick(empty.get("assets"), "eh3d-node-bungard", "s3-eth").image_url.empty());
  // the machine node does not take the image of the drill head even when it is the only one there
  json::Value only_aux;
  CHECK(json::parse("{\"assets\":[" + asset("eh3d-node-bungard-aux-0.0.1-s3-eth-app.bin") + "]}", only_aux));
  CHECK(release::pick(only_aux.get("assets"), "eh3d-node-bungard", "s3-eth").image_url.empty());
  // the image without its checksum is found, and the node will refuse it for that
  json::Value no_sum;
  CHECK(json::parse("{\"assets\":[" + asset("eh3d-node-bungard-0.0.4-s3-eth-app.bin") + "]}", no_sum));
  release::Picked bare = release::pick(no_sum.get("assets"), "eh3d-node-bungard", "s3-eth");
  CHECK(!bare.image_url.empty() && bare.checksum_url.empty());
}

static void buffers_and_errors() {
  CHECK(release::header_buffer_size(0) == release::kHeaderBufferInternal);
  CHECK(release::header_buffer_size(100000) == release::kHeaderBufferExternal);
  CHECK(release::fetch_error("http_404") == "http_404" && release::fetch_error("too_large") == "checksum_too_large" && release::fetch_error("connect_failed") == "checksum_unreachable");
}

static void versions() {
  CHECK(semver::is_newer("0.0.4", "0.0.3") && semver::is_newer("v0.1.0", "0.0.9") && semver::is_newer("0.8.0.1", "0.8.0"));
  CHECK(!semver::is_newer("0.0.3", "0.0.3") && !semver::is_newer("0.0.2", "0.0.3"));
  CHECK(!semver::is_newer("0.0.4-rc1", "0.0.3") && !semver::is_newer("junk", "0.0.3"));   // a release that is not a plain version is never offered
}

int main() {
  official_addresses();
  picking_the_image();
  buffers_and_errors();
  versions();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
