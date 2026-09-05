/*
 * Copyright 2025 LiveKit
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

#include "livekit/video_source.h"

#include <stdexcept>

#include "ffi.pb.h"
#include "ffi_client.h"
#include "livekit/video_frame.h"
#include "video_frame.pb.h"
#include "video_utils.h"

namespace livekit {

VideoSource::VideoSource(int width, int height) : VideoSource(width, height, false) {}

VideoSource::VideoSource(int width, int height, bool pre_encoded) : width_(width), height_(height) {
  proto::FfiRequest req;
  auto* msg = req.mutable_new_video_source();
  msg->set_type(pre_encoded ? proto::VideoSourceType::VIDEO_SOURCE_NATIVE_ENCODED
                            : proto::VideoSourceType::VIDEO_SOURCE_NATIVE);
  msg->mutable_resolution()->set_width(width_);
  msg->mutable_resolution()->set_height(height_);

  const proto::FfiResponse resp = FfiClient::instance().sendRequest(req);
  if (!resp.has_new_video_source()) {
    throw std::runtime_error("VideoSource: missing new_video_source");
  }

  handle_ = FfiHandle(resp.new_video_source().source().handle().id());
}

void VideoSource::captureFrame(const VideoFrame& frame, const VideoCaptureOptions& options) {
  if (!handle_) {
    return;
  }

  const proto::VideoBufferInfo buf = toProto(frame);
  proto::FfiRequest req;
  auto* msg = req.mutable_capture_video_frame();
  msg->set_source_handle(handle_.get());
  msg->mutable_buffer()->CopyFrom(buf);
  msg->set_timestamp_us(options.timestamp_us);
  msg->set_rotation(static_cast<proto::VideoRotation>(options.rotation));
  if (auto metadata = toProto(options.metadata)) {
    msg->mutable_metadata()->CopyFrom(*metadata);
  }
  const proto::FfiResponse resp = FfiClient::instance().sendRequest(req);
  if (!resp.has_capture_video_frame()) {
    throw std::runtime_error("FfiResponse missing capture_video_frame");
  }
}

void VideoSource::captureFrame(const VideoFrame& frame, std::int64_t timestamp_us, VideoRotation rotation) {
  captureFrame(frame, VideoCaptureOptions{timestamp_us, rotation, {}});
}

EncodedVideoSource::EncodedVideoSource(int width, int height) : VideoSource(width, height, true) {}

bool EncodedVideoSource::captureFrame(const EncodedVideoFrame& frame) {
  if (!frame.data || frame.size == 0) {
    throw std::invalid_argument("EncodedVideoSource: frame payload is empty");
  }
  if (ffiHandleId() == 0) return false;

  proto::FfiRequest req;
  auto* msg = req.mutable_capture_encoded_video_frame();
  msg->set_source_handle(ffiHandleId());
  msg->set_data_ptr(reinterpret_cast<std::uint64_t>(frame.data));
  msg->set_data_size(static_cast<std::uint64_t>(frame.size));
  msg->set_timestamp_us(frame.timestamp_us);
  msg->set_key_frame(frame.key_frame);
  if (auto metadata = toProto(frame.metadata)) {
    msg->mutable_metadata()->CopyFrom(*metadata);
  }
  const proto::FfiResponse resp = FfiClient::instance().sendRequest(req);
  if (!resp.has_capture_encoded_video_frame()) {
    throw std::runtime_error("FfiResponse missing capture_encoded_video_frame");
  }
  return resp.capture_encoded_video_frame().accepted();
}

bool EncodedVideoSource::takeKeyFrameRequest() {
  if (ffiHandleId() == 0) return false;
  proto::FfiRequest req;
  req.mutable_take_video_key_frame_request()->set_source_handle(ffiHandleId());
  const proto::FfiResponse resp = FfiClient::instance().sendRequest(req);
  if (!resp.has_take_video_key_frame_request()) {
    throw std::runtime_error("FfiResponse missing take_video_key_frame_request");
  }
  return resp.take_video_key_frame_request().requested();
}

} // namespace livekit
