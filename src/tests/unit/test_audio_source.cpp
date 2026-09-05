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

#include <gtest/gtest.h>
#include <livekit/audio_source.h>
#include <livekit/livekit.h>

#include <system_error>

namespace livekit::test {

class AudioSourceTest : public ::testing::Test {
protected:
  void SetUp() override { livekit::initialize(livekit::LogLevel::Info); }
  void TearDown() override { livekit::shutdown(); }
};

TEST_F(AudioSourceTest, ConstructAndQueryProperties) {
  AudioSource source(48000, 1);
  EXPECT_EQ(source.sampleRate(), 48000);
  EXPECT_EQ(source.numChannels(), 1);
  EXPECT_NE(source.ffiHandleId(), 0u);
  EXPECT_DOUBLE_EQ(source.queuedDuration(), 0.0);
}

TEST_F(AudioSourceTest, ClearQueueIsSafeOnFreshSource) {
  AudioSource source(48000, 2, /*queue_size_ms=*/0);
  source.clearQueue();
  EXPECT_DOUBLE_EQ(source.queuedDuration(), 0.0);
}

TEST_F(AudioSourceTest, RejectsNegativeTimeoutBeforeAdmission) {
  AudioSource source(48000, 1);
  auto frame = AudioFrame::create(48000, 1, 480);
  EXPECT_THROW(source.captureFrame(frame, -1), std::invalid_argument);
  EXPECT_DOUBLE_EQ(source.queuedDuration(), 0.0);
}

TEST_F(AudioSourceTest, BufferedIngressTimeoutIsNotReportedAsSuccess) {
  AudioSource source(48000, 1, 10);
  auto frame = AudioFrame::create(48000, 1, 48000);
  // One second of PCM cannot drain through a 10 ms real-time queue within
  // the 1 ms callback deadline. PCM has already been copied by the Rust FFI.
  try {
    source.captureFrame(frame, 1);
    ADD_FAILURE() << "Timed-out capture returned success";
  } catch (const std::system_error& error) {
    EXPECT_EQ(error.code(), std::make_error_code(std::errc::timed_out));
  }
  source.clearQueue();
}

} // namespace livekit::test
