/*
 * Copyright 2026 LiveKit, Inc.
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

#include <cstdint>
#include <limits>

#include "../../../client-sdk-rust/webrtc-sys/include/livekit/encoded_reference_continuity.h"

TEST(EncodedReferenceContinuity, StartsOnlyWithKeyframe) {
  livekit::EncodedReferenceContinuity state;
  EXPECT_FALSE(state.admit(1, false));
  EXPECT_FALSE(state.admit(2, false));
  EXPECT_TRUE(state.admit(3, true));
  EXPECT_TRUE(state.admit(4, false));
}

TEST(EncodedReferenceContinuity, MissingReferenceWaitsForFreshKeyframe) {
  livekit::EncodedReferenceContinuity state;
  ASSERT_TRUE(state.admit(1, true));
  ASSERT_TRUE(state.admit(2, false));
  EXPECT_FALSE(state.admit(4, false));
  for (std::uint64_t sequence = 5; sequence < 100; ++sequence) EXPECT_FALSE(state.admit(sequence, false));
  EXPECT_TRUE(state.admit(100, true));
  EXPECT_TRUE(state.admit(101, false));
}

TEST(EncodedReferenceContinuity, MissingFramesBeforeKeyframeNeedNoExtraWait) {
  livekit::EncodedReferenceContinuity state;
  ASSERT_TRUE(state.admit(1, true));
  EXPECT_TRUE(state.admit(10, true));
  EXPECT_TRUE(state.admit(11, false));
}

TEST(EncodedReferenceContinuity, CallbackFailureInvalidatesReference) {
  livekit::EncodedReferenceContinuity state;
  ASSERT_TRUE(state.admit(1, true));
  state.invalidate();
  EXPECT_FALSE(state.admit(2, false));
  EXPECT_TRUE(state.admit(3, true));
}

TEST(EncodedReferenceContinuity, RejectsDuplicateReorderedAndOverflowedFrames) {
  livekit::EncodedReferenceContinuity state;
  EXPECT_FALSE(state.admit(0, true));
  ASSERT_TRUE(state.admit(2, true));
  EXPECT_FALSE(state.admit(2, true));
  EXPECT_FALSE(state.admit(1, true));
  EXPECT_FALSE(state.admit(3, false));
  EXPECT_TRUE(state.admit((std::numeric_limits<std::uint64_t>::max)(), true));
  EXPECT_FALSE(state.admit(0, false));
}

TEST(EncodedReferenceContinuity, EncoderAndSourceReplacementResetState) {
  livekit::EncodedReferenceContinuity state;
  ASSERT_TRUE(state.admit(100, true));
  state = {};
  EXPECT_FALSE(state.admit(1, false));
  EXPECT_TRUE(state.admit(2, true));
}
