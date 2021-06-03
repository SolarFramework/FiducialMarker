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

#include <core/Log.h>

#include <api/pipeline/IPoseEstimationPipeline.h>
#include <api/display/IImageViewer.h>
#include <api/display/I3DOverlay.h>
#include <datastructure/CameraDefinitions.h>

#include <xpcf/xpcf.h>

#include <boost/log/core.hpp>
#include <gtest/gtest.h>

namespace xpcf  = org::bcom::xpcf;

using namespace SolAR;
using namespace SolAR::datastructure;
using namespace SolAR::api;

/*
 * Example of fixture to test Pipeline init function with
 * various parameters without writing the same initialization
 * code several times.
 */
class PipelineInitFixture : public testing::Test
{
protected:
    void SetUp() override
    {
        LOG_ADD_LOG_TO_CONSOLE();

        componentMgr = xpcf::getComponentManagerInstance();

        // Required to run several tests with same mngr instance
        componentMgr->clear();

        componentMgr->load("SolARPipelineTest_FiducialMarker_conf_test0001.xml");

        pipeline = componentMgr->resolve<pipeline::IPoseEstimationPipeline>();
    }

    void TearDown() override {}

    SRef<xpcf::IComponentManager> componentMgr = nullptr;
    SRef<pipeline::IPoseEstimationPipeline> pipeline = nullptr;
};

TEST_F(PipelineInitFixture, testPipelineInit)
{
    ASSERT_EQ(pipeline->init(componentMgr), FrameworkReturnCode::_SUCCESS);
}

TEST_F(PipelineInitFixture, testPipelineInitWithNull)
{
    GTEST_SKIP() << "Ignore, TODO check init() for nullptr";
    ASSERT_EQ(pipeline->init(nullptr), FrameworkReturnCode::_ERROR_);
}


TEST(SolARPipelineTest_FiducialMarker, testNonExistingConfigurationThrows)
{
    LOG_ADD_LOG_TO_CONSOLE();
    auto componentMgr = xpcf::getComponentManagerInstance();
    // Required to run several tests with same mngr instance
    componentMgr->clear();

    EXPECT_EQ(componentMgr->load("bogus.xml"), xpcf::XPCFErrorCode::_FAIL);

    try
    {
        auto pipeline = componentMgr->resolve<pipeline::IPoseEstimationPipeline>();
        FAIL() << "An exception should have been thrown";
    }
    catch(xpcf::Exception e)
    {
        ASSERT_EQ(e.getErrorCode(), xpcf::XPCFErrorCode::_ERROR_INJECTABLE_NOBIND);
    }
}

