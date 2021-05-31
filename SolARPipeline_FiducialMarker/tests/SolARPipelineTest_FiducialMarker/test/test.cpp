/**
 * @copyright Copyright (c) 2021 B-com http://www.b-com.com/
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

#include "SolARPipelineTest_FiducialMarker.h"

#include <gtest/gtest.h>

TEST(SolARPipelineTest_FiducialMarker, testNominalPlayback)
{
    auto builder = SolARPipelineTest_FiducialMarker::Builder()
                    .selectPlaybackMode("SolARPipelineTest_FiducialMarker_conf_test0001.xml",
                        "SolARPipeline_FiducialMarker_test_0001.mp4",
                        2);

    std::shared_ptr<SolARPipelineTest_FiducialMarker> prog;
    ASSERT_NO_THROW(prog = builder.build());

    EXPECT_EQ(prog->pipeline_test_main(), 0);

    EXPECT_TRUE(prog->isPoseDetected());
}

TEST(SolARPipelineTest_FiducialMarker, testEmptyConfiguration)
{
    auto builder = SolARPipelineTest_FiducialMarker::Builder()
                    .selectPlaybackMode("",
                        "",
                        -1);

    ASSERT_THROW(builder.build(), std::runtime_error);
}

// Add tests with other videos, configurations, ...








