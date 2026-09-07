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
#include <cstdint>
#include <memory>
#include <thread>

#include "livekit/visibility.h"

namespace livekit {

/// @brief Result of an exclusively owned fixed-frame DSP operation.
enum class RealtimeAudioProcessingResult { ok, invalid_input, failed, wrong_thread };

/// @brief Warm, fixed 48 kHz mono microphone AEC/NS, independent of AudioSource.
///
/// Construct, process and destroy on one owner thread. Construction
/// allocates and prewarms both processors; ordinary processing uses fixed
/// buffers and bypasses the protobuf FFI and global handle table. NS and AEC
/// can be enabled independently per frame. No Room or device is owned here.
class LIVEKIT_API RealtimeAudioProcessing final {
public:
  /// @brief Allocate and prewarm the two independent WebRTC processors.
  /// @throws std::runtime_error If processor initialization fails.
  RealtimeAudioProcessing();
  /// @brief Release the processors on their owner after processing has stopped.
  ~RealtimeAudioProcessing();
  RealtimeAudioProcessing(const RealtimeAudioProcessing&) = delete;
  RealtimeAudioProcessing& operator=(const RealtimeAudioProcessing&) = delete;

  /// @brief Process one fixed frame in place, AEC followed by NS.
  /// @param microphone Exactly 480 writable mono PCM16 samples.
  /// @param reference Rendered mono frame, or null to bypass unavailable AEC.
  /// @param stream_delay_ms Estimated render/capture delay, between 0 and 500 ms.
  /// @param noise_suppression Whether to apply the warm NS processor.
  /// @param echo_cancellation Whether to apply AEC when reference is present.
  /// @return Status; a failure does not change any device or publication.
  [[nodiscard]] RealtimeAudioProcessingResult process(std::array<std::int16_t, 480>& microphone,
                                                      const std::array<std::int16_t, 480>* reference,
                                                      std::uint32_t stream_delay_ms, bool noise_suppression,
                                                      bool echo_cancellation) noexcept;

private:
  struct Deleter {
    void operator()(void* value) const noexcept;
  };
  std::unique_ptr<void, Deleter> state_;
  std::thread::id owner_;
};
} // namespace livekit
