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

#include "livekit/realtime_audio_processing.h"

#include <stdexcept>

// Private native ABI: no generated protocol changes or public raw pointer API.
extern "C" {
void* livekit_ffi_realtime_apm_create();
void livekit_ffi_realtime_apm_destroy(void* handle);
int livekit_ffi_realtime_apm_process(void* handle, std::int16_t* microphone, std::size_t samples,
                                     const std::int16_t* reference, std::uint32_t delay_ms, bool noise_suppression,
                                     bool echo_cancellation);
}

namespace livekit {
namespace {
RealtimeAudioProcessingResult resultOf(int result) noexcept {
  if (result == 0) return RealtimeAudioProcessingResult::ok;
  if (result == 1) return RealtimeAudioProcessingResult::invalid_input;
  return RealtimeAudioProcessingResult::failed;
}
} // namespace
void RealtimeAudioProcessing::Deleter::operator()(void* value) const noexcept {
  livekit_ffi_realtime_apm_destroy(value);
}
RealtimeAudioProcessing::RealtimeAudioProcessing()
    : state_(livekit_ffi_realtime_apm_create()), owner_(std::this_thread::get_id()) {
  if (!state_) throw std::runtime_error("Realtime microphone DSP initialization failed");
}
RealtimeAudioProcessing::~RealtimeAudioProcessing() {
  if (owner_ != std::this_thread::get_id()) std::terminate();
}
RealtimeAudioProcessingResult RealtimeAudioProcessing::process(std::array<std::int16_t, 480>& microphone,
                                                               const std::array<std::int16_t, 480>* reference,
                                                               std::uint32_t stream_delay_ms, bool noise_suppression,
                                                               bool echo_cancellation) noexcept {
  if (owner_ != std::this_thread::get_id()) return RealtimeAudioProcessingResult::wrong_thread;
  return resultOf(livekit_ffi_realtime_apm_process(state_.get(), microphone.data(), microphone.size(),
                                                   reference ? reference->data() : nullptr, stream_delay_ms,
                                                   noise_suppression, echo_cancellation));
}
} // namespace livekit
