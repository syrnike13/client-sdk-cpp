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

#include "livekit/fresh_audio_stream.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace {
struct NativeFreshAudioInfo {
  std::uint64_t sequence;
  std::uint64_t age_us;
  std::uint64_t received;
  std::uint64_t dropped;
  std::uint64_t stale;
  std::uint64_t invalid;
  std::uint64_t queued;
  std::uint32_t discontinuity;
};
} // namespace

extern "C" {
void* livekit_ffi_fresh_audio_create(std::uint64_t track_handle);
void livekit_ffi_fresh_audio_destroy(void* handle);
int livekit_ffi_fresh_audio_read(void* handle, std::int16_t* samples, std::size_t sample_count,
                                 std::uint64_t maximum_age_us, NativeFreshAudioInfo* info);
}

namespace livekit {
void FreshAudioStream::Deleter::operator()(void* value) const noexcept { livekit_ffi_fresh_audio_destroy(value); }

FreshAudioStream::FreshAudioStream(std::shared_ptr<Track> track) : track_(std::move(track)) {
  if (!track_ || track_->kind() != TrackKind::KIND_AUDIO)
    throw std::invalid_argument("Fresh audio stream requires an audio track");
  state_.reset(livekit_ffi_fresh_audio_create(track_->ffiHandleId()));
  if (!state_) throw std::runtime_error("Fresh audio stream initialization failed");
}

FreshAudioStream::~FreshAudioStream() {
  if (owner_ != std::this_thread::get_id()) std::terminate();
}

FreshAudioReadResult FreshAudioStream::tryRead(FreshAudioFrame& output, std::uint64_t maximum_age_us) noexcept {
  if (owner_ != std::this_thread::get_id()) return FreshAudioReadResult::wrong_thread;
  if (!maximum_age_us || maximum_age_us > 1'000'000) return FreshAudioReadResult::invalid_input;
  FreshAudioFrame next;
  NativeFreshAudioInfo info{};
  const auto read_started = std::chrono::steady_clock::now();
  const auto result =
      livekit_ffi_fresh_audio_read(state_.get(), next.samples.data(), next.samples.size(), maximum_age_us, &info);
  if (result < 0) return FreshAudioReadResult::invalid_input;
  stats_ = {info.received, info.dropped, info.stale + bridge_stale_, info.invalid, info.queued};
  if (!result) return FreshAudioReadResult::empty;
  next.sequence = info.sequence;
  if (info.age_us > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
    return FreshAudioReadResult::invalid_input;
  // The Rust age is sampled during the native call. Subtract it from the call's
  // START time: bridge time may conservatively increase age, never hide a stall.
  next.decoded_at = read_started - std::chrono::microseconds{static_cast<std::int64_t>(info.age_us)};
  next.age_us = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - next.decoded_at)
          .count());
  if (next.age_us > maximum_age_us) {
    ++bridge_stale_;
    ++stats_.stale;
    return FreshAudioReadResult::empty;
  }
  next.discontinuity = info.discontinuity != 0;
  output = next;
  return FreshAudioReadResult::frame;
}
} // namespace livekit
