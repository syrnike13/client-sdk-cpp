/*
 * Copyright 2026 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include "livekit/track.h"
#include "livekit/visibility.h"

namespace livekit {

struct FreshAudioFrame {
  std::array<std::int16_t, 960> samples{};
  std::uint64_t sequence = 0;
  /// Elapsed monotonic time since decoded native sink ingress, before queues.
  std::uint64_t age_us = 0;
  /// Conservative timestamp in the consuming C++ process's steady-clock domain.
  /// Preserve this value through application queues instead of re-stamping PCM.
  std::chrono::steady_clock::time_point decoded_at{};
  bool discontinuity = false;
};

struct FreshAudioStreamStats {
  std::uint64_t received = 0;
  std::uint64_t dropped = 0;
  std::uint64_t stale = 0;
  std::uint64_t invalid = 0;
  std::uint64_t queued = 0;
};

enum class FreshAudioReadResult { frame, empty, invalid_input, wrong_thread };

/// @brief Track-bound 48 kHz stereo/10 ms decoded ingress for native playout.
/// Four preallocated frames; overload drops oldest, contention never waits.
/// No per-frame FFI event, callback thread, or legacy AudioStream queue.
/// Construct/read/destroy on one owner, before Room/SDK teardown. Copy the
/// returned decoded_at through the application's own queues unchanged.
/// NativeAudioSink format conversion is included in the age. It may initialize
/// its resampler on a format change; this API bounds the post-conversion queue.
class LIVEKIT_API FreshAudioStream final {
public:
  explicit FreshAudioStream(std::shared_ptr<Track> track);
  ~FreshAudioStream();
  FreshAudioStream(const FreshAudioStream&) = delete;
  FreshAudioStream& operator=(const FreshAudioStream&) = delete;
  FreshAudioStream(FreshAudioStream&&) = delete;
  FreshAudioStream& operator=(FreshAudioStream&&) = delete;

  /// Read once without waiting. Maximum age must be 1..1,000,000 microseconds.
  /// On empty/error, output is unchanged. Stats are refreshed by valid reads.
  [[nodiscard]] FreshAudioReadResult tryRead(FreshAudioFrame& output, std::uint64_t maximum_age_us = 60'000) noexcept;
  [[nodiscard]] FreshAudioStreamStats stats() const noexcept { return stats_; }

private:
  struct Deleter {
    void operator()(void* value) const noexcept;
  };
  std::shared_ptr<Track> track_;
  std::unique_ptr<void, Deleter> state_;
  const std::thread::id owner_ = std::this_thread::get_id();
  FreshAudioStreamStats stats_;
  std::uint64_t bridge_stale_ = 0;
};
} // namespace livekit
