#include "VisionA.h"
#include "Publish.h"
#include "Config.h"
#include "Types.h"
#include "FrameProcessor.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <string>
#include <algorithm>
#include <set>
#include <iostream>
#include <chrono>
#include <fstream>
#include <cstddef>

namespace vision {
class VisionClient {
public:
    explicit VisionClient();
    ~VisionClient() = default;

    // 运行VisionClient
    int runVision(
        const std::string& frame_src = "./assets/vision/videos/demo.mp4",  // frame / video src
        const std::string& output_file = "./out/000000.jsonl",             // output file default as ./out/xxxxxx.jsonl
        const std::string& max_process_frames = "500",                     // max process frames default as 500
        const std::string& sample_fps = "0.5",                             // fps sampling default as one frame per 2 seconds, requires video fps to judge, need extra judging method and mechanism
        const std::string& is_stream_process = "true"                      // stream process or not, default use stream process
    );

    // now_t_ms
    int64_t now_ms();

};
}
