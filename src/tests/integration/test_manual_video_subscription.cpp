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

#include <livekit/livekit.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "tests/common/test_common.h"

namespace livekit::test {
namespace {
class PublicationObserver final : public RoomDelegate {
public:
  void onTrackPublished(Room&, const TrackPublishedEvent& event) override {
    const std::scoped_lock lock(mutex);
    published = true;
    publication = event.publication;
    changed.notify_all();
  }

  void onTrackSubscribed(Room&, const TrackSubscribedEvent& event) override {
    const std::scoped_lock lock(mutex);
    subscribed_publication = event.publication;
    track = event.track;
    changed.notify_all();
  }

  std::mutex mutex;
  std::condition_variable changed;
  bool published = false;
  std::shared_ptr<RemoteTrackPublication> publication;
  std::shared_ptr<RemoteTrackPublication> subscribed_publication;
  std::shared_ptr<Track> track;
};

// Closing the bounded stream wakes read(), including on assertion/exception exits.
class FrameReader final {
public:
  explicit FrameReader(const std::shared_ptr<Track>& track)
      : stream_(VideoStream::fromTrack(track, {1, VideoBufferType::BGRA})), reader_([this] {
          VideoFrameEvent event;
          if (stream_->read(event)) {
            const std::scoped_lock lock(mutex_);
            valid_ = event.frame.width() == 640 && event.frame.height() == 360 &&
                     event.frame.type() == VideoBufferType::BGRA &&
                     event.frame.dataSize() == std::size_t{640} * 360 * 4;
          }
        }) {}

  ~FrameReader() {
    stream_->close();
    reader_.join();
  }

  bool valid() {
    const std::scoped_lock lock(mutex_);
    return valid_;
  }

private:
  std::mutex mutex_;
  bool valid_ = false;
  std::shared_ptr<VideoStream> stream_;
  std::thread reader_;
};
} // namespace

class ManualVideoSubscriptionTest : public LiveKitTestBase {};

TEST_F(ManualVideoSubscriptionTest, PublishedEventRetainsPublicationAndEnablesDecodedVideo) {
  failIfNotConfigured();
  PublicationObserver observer;
  Room receiver;
  Room sender;
  receiver.setDelegate(&observer);
  RoomOptions options;
  options.auto_subscribe = false;
  ASSERT_TRUE(receiver.connect(config_.url, config_.token_b, options));
  ASSERT_TRUE(sender.connect(config_.url, config_.token_a, options));

  auto source = std::make_shared<VideoSource>(640, 360);
  auto local_track = LocalVideoTrack::createLocalVideoTrack("manual-video", source);
  TrackPublishOptions publish_options;
  publish_options.source = TrackSource::SOURCE_CAMERA;
  publish_options.simulcast = false;
  lockLocalParticipant(sender)->publishTrack(local_track, publish_options);

  std::shared_ptr<RemoteTrackPublication> publication;
  {
    std::unique_lock<std::mutex> lock(observer.mutex);
    ASSERT_TRUE(observer.changed.wait_for(lock, 10s, [&] { return observer.published; }));
    publication = observer.publication;
    ASSERT_EQ(observer.track, nullptr);
  }
  ASSERT_NE(publication, nullptr);
  ASSERT_NE(local_track->publication(), nullptr);
  EXPECT_EQ(publication->sid(), local_track->publication()->sid());
  EXPECT_EQ(publication->name(), "manual-video");
  // Synchronous FFI requests belong on the application thread, outside the callback.
  publication->setSubscribed(true);
  std::shared_ptr<Track> remote_track;
  {
    std::unique_lock<std::mutex> lock(observer.mutex);
    ASSERT_TRUE(observer.changed.wait_for(lock, 10s, [&] { return observer.track != nullptr; }));
    EXPECT_EQ(observer.subscribed_publication, publication);
    remote_track = observer.track;
  }
  EXPECT_EQ(remote_track->sid(), local_track->publication()->sid());
  {
    FrameReader reader(remote_track);
    auto frame = VideoFrame::create(640, 360, VideoBufferType::RGBA);
    std::fill(frame.data(), frame.data() + frame.dataSize(), 0x7f);
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!reader.valid() && std::chrono::steady_clock::now() < deadline) {
      VideoCaptureOptions capture_options;
      capture_options.timestamp_us = static_cast<std::int64_t>(getTimestampUs());
      source->captureFrame(frame, capture_options);
      std::this_thread::sleep_for(33ms);
    }
    EXPECT_TRUE(reader.valid()) << "Manual subscription did not deliver a decoded BGRA frame";
  }
  EXPECT_TRUE(sender.disconnect());
  EXPECT_TRUE(receiver.disconnect());
}
} // namespace livekit::test
