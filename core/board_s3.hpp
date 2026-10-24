// EH3D-NODE-BUNGARD - the two boards this firmware is built for, and which GPIO pins of each may be given to the machine (the serial line, the lamps, the spindle).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// One firmware, two board profiles (chosen when the image is built; the host tests build both):
//
//   s3-wifi   an ESP32-S3-DevKitC-1 style ESP32-S3-WROOM-1 N16R8 (16 MB flash, 8 MB octal PSRAM), two USB-C sockets, no Ethernet: Wi-Fi is the only way in.
//   s3-eth    the Waveshare ESP32-S3-ETH: ESP32-S3R8 (8 MB octal PSRAM), 16 MB flash, a W5500 Ethernet controller on SPI (RJ45, and PoE through a separate
//             module), a microSD socket and a camera connector. Ethernet is the way in; Wi-Fi may be kept as an access point.
//
// What both share, from the printed pin maps and the chip's datasheet:
//   - GPIO 22..25 do not exist, and 26..32 are the external flash;
//   - GPIO 33..37 belong to the octal PSRAM (35, 36 and 37 are on the header of the N16R8 module: never use them);
//   - GPIO 19 and 20 are the native USB pair (the log and the flashing);
//   - GPIO 0 (BOOT), 3, 45 and 46 are strapping pins read at reset: usable, but whatever is wired there must not pull them the wrong way while the chip starts;
//   - GPIO 39..42 are the JTAG pins of the chip; they are free unless a JTAG probe is used.
// Only on s3-wifi: GPIO 43 and 44 are UART0's pins to the CH343P of the "USB to serial" socket (usable, a cable in that socket drives them) and GPIO 48
// drives the on-board RGB LED (usable, the LED flickers).
// Only on s3-eth: GPIO 9..14 are the W5500 (reset, interrupt, MOSI, MISO, clock, chip select): never offered. GPIO 8 goes to the camera connector and 4..7 to the microSD
// socket: usable when no camera and no card are used (this node uses neither).
// Nothing here has been checked on a board.
#pragma once
#include <cstdint>

namespace eh3d::board {

#if defined(EH3D_BOARD_S3_ETH)
constexpr bool kHasEthernet = true;
constexpr const char* kId = "s3-eth";
constexpr const char* kName = "Waveshare ESP32-S3-ETH";
#else
constexpr bool kHasEthernet = false;
constexpr const char* kId = "s3-wifi";
constexpr const char* kName = "ESP32-S3-WROOM-1 N16R8";
#endif

enum class PinUse : std::uint8_t {
  kFree,      // no special role
  kCaution,   // a strapping pin, the boot button, the USB-serial pair, the LED or the SD socket: usable with care
  kReserved,  // taken by the board or by the chip: never offered
};

enum class Reserved : std::uint8_t { kNone, kNoSuchPin, kFlash, kPsram, kUsb, kEthernet, kCamera };

struct PinInfo {
  int gpio;
  PinUse use;
  Reserved reason;   // only meaningful for kReserved
  const char* note;  // "" or a short code the panel translates: "strapping", "boot", "usb_serial", "led", "sdcard"
  bool on_header;
};

constexpr int kFirstGpio = 0;
constexpr int kLastGpio = 48;

constexpr PinInfo pin_info(int gpio) {
  if (gpio < kFirstGpio || gpio > kLastGpio || (gpio >= 22 && gpio <= 25)) return {gpio, PinUse::kReserved, Reserved::kNoSuchPin, "", false};
  if (gpio >= 26 && gpio <= 32) return {gpio, PinUse::kReserved, Reserved::kFlash, "", false};
  if (gpio >= 33 && gpio <= 37) return {gpio, PinUse::kReserved, Reserved::kPsram, "", gpio >= 35 && !kHasEthernet};
  if (gpio == 19 || gpio == 20) return {gpio, PinUse::kReserved, Reserved::kUsb, "", true};
  if (gpio == 0) return {gpio, PinUse::kCaution, Reserved::kNone, "boot", true};
  if (gpio == 3 || gpio == 45 || gpio == 46) return {gpio, PinUse::kCaution, Reserved::kNone, "strapping", true};
#if defined(EH3D_BOARD_S3_ETH)
  if (gpio >= 9 && gpio <= 14) return {gpio, PinUse::kReserved, Reserved::kEthernet, "", false};
  if (gpio == 8) return {gpio, PinUse::kCaution, Reserved::kNone, "camera", true};
  if (gpio >= 4 && gpio <= 7) return {gpio, PinUse::kCaution, Reserved::kNone, "sdcard", true};
#else
  if (gpio == 43 || gpio == 44) return {gpio, PinUse::kCaution, Reserved::kNone, "usb_serial", true};
  if (gpio == 48) return {gpio, PinUse::kCaution, Reserved::kNone, "led", true};
#endif
  return {gpio, PinUse::kFree, Reserved::kNone, "", true};
}

constexpr bool assignable(int gpio) { return pin_info(gpio).use != PinUse::kReserved; }

}  // namespace eh3d::board
