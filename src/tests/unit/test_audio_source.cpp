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
#include <livekit/fresh_audio_stream.h>
#include <livekit/livekit.h>
#include <livekit/local_audio_track.h>

#include <algorithm>
#include <chrono>
#include <system_error>
#include <thread>

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

TEST_F(AudioSourceTest, FreshSinkBoundsQueueAndPreservesStereo) {
  auto source = std::make_shared<AudioSource>(48000, 2, 0);
  auto track = LocalAudioTrack::createLocalAudioTrack("fresh-input", source);
  FreshAudioStream stream(track);
  auto input = AudioFrame::create(48000, 2, 480);
  for (std::size_t sample = 0; sample < input.data().size(); sample += 2) {
    input.data()[sample] = 1234;
    input.data()[sample + 1] = -2345;
  }
  for (int frame = 0; frame < 12; ++frame) source->captureFrame(input, 1000);
  FreshAudioFrame output;
  ASSERT_EQ(stream.tryRead(output, 1'000'000), FreshAudioReadResult::frame);
  EXPECT_EQ(output.samples.front(), 1234);
  EXPECT_EQ(output.samples.back(), -2345);
  EXPECT_TRUE(output.discontinuity);
  EXPECT_LE(stream.stats().queued, 3u);
  EXPECT_GE(stream.stats().dropped, 8u);
  EXPECT_EQ(stream.tryRead(output, 0), FreshAudioReadResult::invalid_input);
  FreshAudioReadResult foreign = FreshAudioReadResult::frame;
  std::thread reader([&] { foreign = stream.tryRead(output); });
  reader.join();
  EXPECT_EQ(foreign, FreshAudioReadResult::wrong_thread);
}

TEST_F(AudioSourceTest, FreshSinkExpiresOldFramesAndConvertsFormat) {
  auto source = std::make_shared<AudioSource>(16000, 1, 0);
  auto track = LocalAudioTrack::createLocalAudioTrack("converted-input", source);
  FreshAudioStream stream(track);
  auto input = AudioFrame::create(16000, 1, 160);
  std::fill(input.data().begin(), input.data().end(), 2000);
  for (int frame = 0; frame < 4; ++frame) source->captureFrame(input, 1000);
  // An intentional consumer stall must expire queued PCM, not refresh its age
  // at read time. This duration is the injected fault, not readiness polling.
  std::this_thread::sleep_for(std::chrono::milliseconds(70));
  FreshAudioFrame output;
  EXPECT_EQ(stream.tryRead(output, 60'000), FreshAudioReadResult::empty);
  EXPECT_GT(stream.stats().stale, 0u);
  source->captureFrame(input, 1000);
  ASSERT_EQ(stream.tryRead(output, 1'000'000), FreshAudioReadResult::frame);
  EXPECT_EQ(output.samples[400], output.samples[401]);
  EXPECT_GT(output.samples[400], 1000);
  EXPECT_TRUE(output.discontinuity);
  EXPECT_EQ(stream.stats().invalid, 0u);
}

TEST_F(AudioSourceTest, FreshSinkRepeatedTeardownDetachesCallback) {
  auto source = std::make_shared<AudioSource>(48000, 1, 0);
  auto track = LocalAudioTrack::createLocalAudioTrack("lifetime-input", source);
  auto input = AudioFrame::create(48000, 1, 480);
  for (int cycle = 0; cycle < 50; ++cycle) {
    {
      FreshAudioStream stream(track);
      source->captureFrame(input, 1000);
      FreshAudioFrame output;
      ASSERT_EQ(stream.tryRead(output, 1'000'000), FreshAudioReadResult::frame);
    }
    // The retired sink must not remain in the native source's callback list.
    source->captureFrame(input, 1000);
  }
}

} // namespace livekit::test
