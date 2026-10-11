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
 * distributed under the License is distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "livekit/visibility.h"

// Application-provided video decoder, e.g. a GPU decoder whose frames stay on
// the application's device. WebRTC keeps its own decoder as the fallback when
// create() returns null or decode() asks for it. Layout must match
// webrtc-sys/include/livekit/external_video_decoder.h.
extern "C" {

#define LK_EXTERNAL_VIDEO_DECODER_ABI 1

enum LkExternalVideoCodec : std::int32_t {
  LK_EXTERNAL_VIDEO_CODEC_H264 = 0,
};

enum LkExternalDecodeResult : std::int32_t {
  LK_EXTERNAL_DECODE_FRAME = 0,     ///< `out->frame` holds a new reference
  LK_EXTERNAL_DECODE_NO_FRAME = 1,  ///< accepted, nothing to show yet
  LK_EXTERNAL_DECODE_ERROR = 2,     ///< undecodable: WebRTC requests a keyframe
  LK_EXTERNAL_DECODE_FALLBACK = 3,  ///< unusable: switch to the built-in decoder
};

struct LkExternalVideoFrame {
  void* frame;  ///< opaque, released with release_frame
  std::int32_t width;
  std::int32_t height;
};

/// Every function may run on any WebRTC decoder thread; one decoder instance
/// is never called concurrently. The table must outlive the SDK.
struct LkExternalVideoDecoder {
  std::uint32_t abi_version;
  void* user;
  void* (*create)(void* user, std::int32_t codec);
  void (*destroy)(void* decoder);
  std::int32_t (*decode)(void* decoder, const std::uint8_t* data, std::size_t size,
                         std::int32_t keyframe, LkExternalVideoFrame* out);
  void (*release_frame)(void* frame);
  /// CPU copy for streams that did not request external frames; 0 on success.
  std::int32_t (*read_i420)(void* frame, std::uint8_t* y, std::int32_t stride_y,
                            std::uint8_t* u, std::int32_t stride_u, std::uint8_t* v,
                            std::int32_t stride_v);
};

}  // extern "C"

namespace livekit {

/// Installs (or with null removes) the decoder for video tracks subscribed
/// afterwards. Video streams opened with `external_frames` receive its frames
/// as VideoBufferType::External without a CPU copy.
LIVEKIT_API void setExternalVideoDecoder(const LkExternalVideoDecoder* decoder);

}  // namespace livekit
