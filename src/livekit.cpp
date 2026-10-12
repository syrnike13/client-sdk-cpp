/*
 * Copyright 2023 LiveKit
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

#include "livekit/livekit.h"

#include <algorithm>

#include "ffi_client.h"
#include "livekit_ffi.h"
#include "lk_log.h"

namespace livekit {

bool initialize(const LogLevel& level) {
  // Initializes logger if singleton instance is not already initialized
  setLogLevel(level);
  auto& ffi_client = FfiClient::instance();
  // Note: capture_logs currently disabled, requires event support in FfiClient
  return ffi_client.initialize(false);
}

void setExternalVideoDecoder(const LkExternalVideoDecoder* decoder) { livekit_ffi_set_external_video_decoder(decoder); }

void setAudioPlayoutDelay(std::chrono::milliseconds delay) {
  livekit_ffi_set_audio_playout_delay(
      static_cast<std::uint32_t>(std::clamp<std::chrono::milliseconds::rep>(delay.count(), 0, 1000)));
}

void setExternalAudioPlayout(bool enabled) { livekit_ffi_set_external_audio_playout(enabled); }

bool pullAudioPlayout() { return livekit_ffi_pull_audio_playout(); }

bool isInitialized() { return FfiClient::instance().isInitialized(); }

void shutdown() {
  FfiClient::instance().shutdown();
  detail::shutdownLogger();
}

} // namespace livekit
