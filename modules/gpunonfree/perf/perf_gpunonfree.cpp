#include "perf_precomp.hpp"

using namespace std;
using namespace testing;
using namespace perf;

//////////////////////////////////////////////////////////////////////
// SURF

DEF_PARAM_TEST_1(Image, string);

PERF_TEST_P(Image, Features2D_SURF,
            Values<string>("gpu/perf/aloe.png"))
{
    declare.time(50.0);

    const cv::Mat img = readImage(GetParam(), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(img.empty());

    if (PERF_RUN_GPU())
    {
        cv::gpu::SURF_GPU d_surf;

        const cv::gpu::GpuMat d_img(img);
        cv::gpu::GpuMat d_keypoints, d_descriptors;

        TEST_CYCLE() d_surf(d_img, cv::gpu::GpuMat(), d_keypoints, d_descriptors);

        std::vector<cv::KeyPoint> gpu_keypoints;
        d_surf.downloadKeypoints(d_keypoints, gpu_keypoints);

        cv::Mat gpu_descriptors(d_descriptors);

        sortKeyPoints(gpu_keypoints, gpu_descriptors);

        SANITY_CHECK_KEYPOINTS(gpu_keypoints);
        SANITY_CHECK(gpu_descriptors, 1e-3);
    }
    else
    {
        cv::SURF surf;

        std::vector<cv::KeyPoint> cpu_keypoints;
        cv::Mat cpu_descriptors;

        TEST_CYCLE() surf(img, cv::noArray(), cpu_keypoints, cpu_descriptors);

        SANITY_CHECK_KEYPOINTS(cpu_keypoints);
        SANITY_CHECK(cpu_descriptors);
    }
}

//////////////////////////////////////////////////////
// VIBE

DEF_PARAM_TEST(Video_Cn, string, int);

PERF_TEST_P(Video_Cn, Video_VIBE,
            Combine(Values("gpu/video/768x576.avi", "gpu/video/1920x1080.avi"),
                    GPU_CHANNELS_1_3_4))
{
    const string inputFile = perf::TestBase::getDataPath(GET_PARAM(0));
    const int cn = GET_PARAM(1);

    cv::VideoCapture cap(inputFile);
    ASSERT_TRUE(cap.isOpened());

    cv::Mat frame;
    cap >> frame;
    ASSERT_FALSE(frame.empty());

    if (cn != 3)
    {
        cv::Mat temp;
        if (cn == 1)
            cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
        else
            cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
        cv::swap(temp, frame);
    }

    if (PERF_RUN_GPU())
    {
        cv::gpu::GpuMat d_frame(frame);
        cv::gpu::VIBE_GPU vibe;
        cv::gpu::GpuMat foreground;

        vibe(d_frame, foreground);

        for (int i = 0; i < 10; ++i)
        {
            cap >> frame;
            ASSERT_FALSE(frame.empty());

            if (cn != 3)
            {
                cv::Mat temp;
                if (cn == 1)
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2GRAY);
                else
                    cv::cvtColor(frame, temp, cv::COLOR_BGR2BGRA);
                cv::swap(temp, frame);
            }

            d_frame.upload(frame);

            startTimer(); next();
            vibe(d_frame, foreground);
            stopTimer();
        }

        GPU_SANITY_CHECK(foreground);
    }
    else
    {
        FAIL_NO_CPU();
    }
}
