// EH3D-NODE-BUNGARD - core/send_loop.hpp: sending a block of bytes down a socket without ever waiting for ever. GPL-3.0-or-later.
//
// A program that stops reading, or a cable that is cut half way, fills the buffer of the connection; a send that waits for room then waits for minutes, and
// while it does the loop of the link neither ticks the session (the idle timeout), nor the drain, nor answers the stop of the panel. Here the wait has a limit: when the
// bytes do not go out in `limit_ms`, the send gives up and the caller drops the connection, which stops the machine (or lets it finish what it holds) as any lost link does.
// The socket and the clock are passed in, so the rule is tested on a computer.
#pragma once

#include <cstddef>
#include <cstdint>

namespace eh3d::bungard {

/// What one try to send gave: the bytes that went (`sent`), or that the connection has no room now (`again`), or that it failed (neither).
struct SendTry {
  long sent = -1;
  bool again = false;
};

/// Sends all `n` bytes with `try_send(data, n)`, waiting with `pause()` when there is no room, for at most `limit_ms` by `now_ms()`. False when it failed or ran out of time.
template <class TrySend, class Now, class Pause>
bool send_within(const std::uint8_t* data, std::size_t n, std::uint32_t limit_ms, TrySend try_send, Now now_ms, Pause pause) {
  const std::uint32_t started = now_ms();
  while (n > 0) {
    const SendTry r = try_send(data, n);
    if (r.sent > 0) {
      data += r.sent;
      n -= static_cast<std::size_t>(r.sent);
      continue;
    }
    if (!r.again) return false;
    if (static_cast<std::uint32_t>(now_ms() - started) >= limit_ms) return false;
    pause();
  }
  return true;
}

}  // namespace eh3d::bungard
