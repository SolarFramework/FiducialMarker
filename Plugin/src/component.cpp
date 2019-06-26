
#include "xpcf/module/ModuleFactory.h"
#include "PipelineFiducialMarker.h"
#include "SolARModuleOpencv_traits.h"
#include "SolARModuleTools_traits.h"
#include "core/Log.h"

XPCF_DEFINE_FACTORY_CREATE_INSTANCE(SolAR::PIPELINES::PipelineFiducialMarker);

namespace SolAR {
    using namespace datastructure;
    using namespace api::pipeline;
    namespace PIPELINES {

    PipelineFiducialMarker::PipelineFiducialMarker():ConfigurableBase(xpcf::toUUID<PipelineFiducialMarker>())
    {
        addInterface<api::pipeline::IPipeline>(this);
        declareInjectable<input::devices::ICamera>(m_camera);
        declareInjectable<input::files::IMarker2DSquaredBinary>(m_binaryMarker);
        declareInjectable<image::IImageFilter>(m_imageFilterBinary);
        declareInjectable<image::IImageConvertor>(m_imageConvertor);
        declareInjectable<features::IContoursExtractor>(m_contoursExtractor);
        declareInjectable<features::IContoursFilter>(m_contoursFilter);
        declareInjectable<image::IPerspectiveController>(m_perspectiveController);
        declareInjectable<features::IDescriptorsExtractorSBPattern>(m_patternDescriptorExtractor);
        declareInjectable<features::IDescriptorMatcher>(m_patternMatcher);
        declareInjectable<features::ISBPatternReIndexer>(m_patternReIndexer);
        declareInjectable<geom::IImage2WorldMapper>(m_img2worldMapper);
        declareInjectable<solver::pose::I3DTransformFinderFrom2D3D>(m_PnP);
        declareInjectable<sink::ISinkPoseImage>(m_sink);
        declareInjectable<source::ISourceImage>(m_source);
        declareInjectable<image::IImageConvertor>(m_imageConvertorUnity,"imageConvertorUnity");

        declareProperty("minThreshold", m_minThreshold);
        declareProperty("maxThreshold", m_maxThreshold);
        declareProperty("nbTestedThreshold", m_nbTestedThreshold);
        declarePropertySequence("LongVector",m_longVect);
        declarePropertySequence("StringVector",m_strVect);
        m_initOK = false;
        m_startedOK = false;
        m_stopFlag = false;
        m_haveToBeFlip = false;
        m_taskProcess = nullptr;
        LOG_DEBUG(" Pipeline constructor");
    }

    PipelineFiducialMarker::~PipelineFiducialMarker()
    {
        if(m_taskProcess != nullptr)
            delete m_taskProcess;
        LOG_DEBUG(" Pipeline destructor")
    }

    FrameworkReturnCode PipelineFiducialMarker::init(SRef<xpcf::IComponentManager> xpcfComponentManager)
    {
        // load marker
        LOG_INFO("LOAD MARKER IMAGE ");
        if( m_binaryMarker->loadMarker()==FrameworkReturnCode::_ERROR_){
            return FrameworkReturnCode::_ERROR_;
        }
        LOG_INFO("MARKER IMAGE LOADED");

        m_patternDescriptorExtractor->extract(m_binaryMarker->getPattern(), m_markerPatternDescriptor);
        LOG_INFO ("Marker pattern:\n {}", m_binaryMarker->getPattern()->getPatternMatrix())

                int patternSize = m_binaryMarker->getPattern()->getSize();

        m_patternDescriptorExtractor->bindTo<xpcf::IConfigurable>()->getProperty("patternSize")->setIntegerValue(patternSize);
        m_patternReIndexer->bindTo<xpcf::IConfigurable>()->getProperty("sbPatternSize")->setIntegerValue(patternSize);

        m_img2worldMapper->bindTo<xpcf::IConfigurable>()->getProperty("digitalWidth")->setIntegerValue(patternSize);
        m_img2worldMapper->bindTo<xpcf::IConfigurable>()->getProperty("digitalHeight")->setIntegerValue(patternSize);
        m_img2worldMapper->bindTo<xpcf::IConfigurable>()->getProperty("worldWidth")->setFloatingValue(m_binaryMarker->getSize().width);
        m_img2worldMapper->bindTo<xpcf::IConfigurable>()->getProperty("worldHeight")->setFloatingValue(m_binaryMarker->getSize().height);

        m_PnP->setCameraParameters(m_camera->getIntrinsicsParameters(), m_camera->getDistorsionParameters());

        for(int i=0;i<4;i++)
            for(int j=0;j<4;j++)
                m_pose(i,j)=0.f;

        m_initOK = true;
        return FrameworkReturnCode::_SUCCESS;
    }

    CameraParameters PipelineFiducialMarker::getCameraParameters()
    {
        CameraParameters camParam;
        // pipeline without camera should not throw ??
        if (m_camera)
        {
            camParam = m_camera->getParameters();
        }
        return camParam;
    }

    bool PipelineFiducialMarker::processCamImage()
    {
        SRef<Image>                     camImage, greyImage, binaryImage;
        std::vector<SRef<Contour2Df>>   contours;
        std::vector<SRef<Contour2Df>>   filtered_contours;
        std::vector<SRef<Image>>        patches;
        std::vector<SRef<Contour2Df>>   recognizedContours;
        SRef<DescriptorBuffer>          recognizedPatternsDescriptors;
        std::vector<DescriptorMatch>    patternMatches;
        std::vector<SRef<Point2Df>>     pattern2DPoints;
        std::vector<SRef<Point2Df>>     img2DPoints;
        std::vector<SRef<Point3Df>>     pattern3DPoints;

        if (m_stopFlag || !m_initOK || !m_startedOK)
            return false;

        bool poseComputed = false;
        if(m_haveToBeFlip)
        {
            m_source->getNextImage(camImage);
        }
        else if (m_camera->getNextImage(camImage) == SolAR::FrameworkReturnCode::_ERROR_LOAD_IMAGE)
        {
            LOG_WARNING("The camera cannot load any image");
            m_stopFlag = true;
            return false;
        }

        if(m_haveToBeFlip)
        {
            m_imageConvertorUnity->convert(camImage,camImage,Image::ImageLayout::LAYOUT_RGB);
        }
        // Convert Image from RGB to grey
        m_imageConvertor->convert(camImage, greyImage, Image::ImageLayout::LAYOUT_GREY);

        for (int num_threshold = 0; !poseComputed && num_threshold < m_nbTestedThreshold; num_threshold++)
        {
            // Compute the current Threshold valu for image binarization
            int threshold = m_minThreshold + (m_maxThreshold - m_minThreshold)*((float)num_threshold/(float)(m_nbTestedThreshold-1));

            // Convert Image from grey to black and white
            m_imageFilterBinary->bindTo<xpcf::IConfigurable>()->getProperty("min")->setIntegerValue(threshold);
            m_imageFilterBinary->bindTo<xpcf::IConfigurable>()->getProperty("max")->setIntegerValue(255);

            // Convert Image from grey to black and white
            m_imageFilterBinary->filter(greyImage, binaryImage);

            // Extract contours from binary image
            m_contoursExtractor->extract(binaryImage, contours);

            // Filter 4 edges contours to find those candidate for marker contours
            m_contoursFilter->filter(contours, filtered_contours);

            // Create one warpped and cropped image by contour
            m_perspectiveController->correct(binaryImage, filtered_contours, patches);

            // test if this last image is really a squared binary marker, and if it is the case, extract its descriptor
            if (m_patternDescriptorExtractor->extract(patches, filtered_contours, recognizedPatternsDescriptors, recognizedContours) != FrameworkReturnCode::_ERROR_)
            {
                // From extracted squared binary pattern, match the one corresponding to the squared binary marker
                if (m_patternMatcher->match(m_markerPatternDescriptor, recognizedPatternsDescriptors, patternMatches) == features::DescriptorMatcher::DESCRIPTORS_MATCHER_OK)
                {
                    // Reindex the pattern to create two vector of points, the first one corresponding to marker corner, the second one corresponding to the poitsn of the contour
                    m_patternReIndexer->reindex(recognizedContours, patternMatches, pattern2DPoints, img2DPoints);

                    // Compute the 3D position of each corner of the marker
                    m_img2worldMapper->map(pattern2DPoints, pattern3DPoints);

                    // Compute the pose of the camera using a Perspective n Points algorithm using only the 4 corners of the marker
                    if (m_PnP->estimate(img2DPoints, pattern3DPoints, m_pose) == FrameworkReturnCode::_SUCCESS)
                    {
                        poseComputed = true;
                    }
                }
            }
        }

        if (poseComputed)
            m_sink->set(m_pose, camImage);
        else
            m_sink->set(camImage);

        return true;
    }

    //////////////////////////////// ADD
    SourceReturnCode PipelineFiducialMarker::loadSourceImage(void* sourceTextureHandle, int width, int height)
    {
        m_haveToBeFlip = true;
        return m_source->setInputTexture((unsigned char *)sourceTextureHandle, width, height);
    }
    ////////////////////////////////////

    FrameworkReturnCode PipelineFiducialMarker::start(void* imageDataBuffer)
    {
        if (m_initOK==false)
        {
            LOG_WARNING("Try to start the Fiducial marker pipeline without initializing it");
            return FrameworkReturnCode::_ERROR_;
        }
        m_stopFlag=false;
        m_sink->setImageBuffer((unsigned char*)imageDataBuffer);

        if (!m_haveToBeFlip && (m_camera->start() != FrameworkReturnCode::_SUCCESS))
        {
            LOG_ERROR("Camera cannot start")
                    return FrameworkReturnCode::_ERROR_;
        }

        // create and start a thread to process the images
        auto processCamImageThread = [this](){;processCamImage();};

        m_taskProcess = new xpcf::DelegateTask(processCamImageThread);
        m_taskProcess->start();
        LOG_INFO("Fiducial marker pipeline has started");
        m_startedOK = true;
        return FrameworkReturnCode::_SUCCESS;
    }

    FrameworkReturnCode PipelineFiducialMarker::stop()
    {
        m_stopFlag=true;

        if( !m_haveToBeFlip)
            m_camera->stop();
        if (m_taskProcess != nullptr)
            m_taskProcess->stop();

        if(!m_initOK)
        {
            LOG_WARNING("Try to stop a pipeline that has not been initialized");
            return FrameworkReturnCode::_ERROR_;
        }
        if (!m_startedOK)
        {
            LOG_WARNING("Try to stop a pipeline that has not been started");
            return FrameworkReturnCode::_ERROR_;
        }

        LOG_INFO("Pipeline has stopped: \n");

        return FrameworkReturnCode::_SUCCESS;
    }

    SinkReturnCode PipelineFiducialMarker::update(Transform3Df& pose)
    {
        return m_sink->tryGet(pose);
    }

    }
}
