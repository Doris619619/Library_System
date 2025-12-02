#include "vision/FrameProcessor.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

// Usage:
//   frame_extract <video_path> [out_dir] [--fps N] [--quality Q] [--start-frame S] [--end-frame E] [--prefix P]
// Examples:
//   frame_extract data/video/sample.mp4 runtime/frames --fps 5 --quality 92 --prefix f_

static bool hasFlag(int argc, char** argv, const std::string& flag) {
    for (int i = 1; i < argc; ++i) if (flag == argv[i]) return true; return false;
}
static const char* getOpt(int argc, char** argv, const std::string& key, const char* defv=nullptr) {
    for (int i = 1; i + 1 < argc; ++i) if (key == argv[i]) return argv[i+1]; return defv;
}

int main(int argc, char** argv) {
    if (argc < 2 || hasFlag(argc, argv, "-h") || hasFlag(argc, argv, "--help")) {
        std::cout << "Usage: frame_extract <video_path> [out_dir] [--fps N] [--quality Q] [--start-frame S] [--end-frame E] [--prefix P]\n";
        return 0;
    }
    std::string videoPath = argv[1];
    std::string outDir = (argc >= 3 && argv[2][0] != '-') ? argv[2] : std::string();
    if (outDir.empty()) {
        std::filesystem::path p(videoPath);
        std::string stem = p.stem().string();
        outDir = (std::filesystem::path("data/frames") / stem).string();
    }

    double fps = 0.0; if (const char* v = getOpt(argc, argv, "--fps"))         fps = std::atof(v);
    int quality = 95; if (const char* v = getOpt(argc, argv, "--quality"))     quality = std::atoi(v);
    int sframe = 0;   if (const char* v = getOpt(argc, argv, "--start-frame")) sframe = std::atoi(v);
    int eframe = -1;  if (const char* v = getOpt(argc, argv, "--end-frame"))   eframe = std::atoi(v);
    std::string prefix = "frame_"; if (const char* v = getOpt(argc, argv, "--prefix")) prefix = v;

    std::cout << "Extracting frames...\n"
              << "  video : " << videoPath << "\n"
              << "  out   : " << outDir << "\n"
              << "  fps   : " << fps << (fps <= 0 ? " (all frames)" : "") << "\n"
              << "  qual  : " << quality << "\n"
              << "  range : [" << sframe << ", " << (eframe < 0 ? -1 : eframe) << "]\n";

    // --- 单帧提取：仅导出视频的第 0 帧到 outDir 下 ---
    auto extractFirstFrame = [](
        const std::string& video_path,
        const std::string& out_dir,
        int jpeg_quality,
        const std::string& filename_prefix
    ) -> size_t {
        cv::VideoCapture cap(video_path);
        if (!cap.isOpened()) {
            std::cerr << "[FrameExtract] Failed to open video: " << video_path << "\n";
            return 0;
        }

        std::error_code error_code;
        if (!std::filesystem::exists(out_dir, error_code)) {
            std::filesystem::create_directories(out_dir, error_code);
            if (error_code) {
                std::cerr << "[FrameExtract] Failed to create out dir: " << out_dir << " : " << error_code.message() << "\n";
                return 0;
            }
        }

        cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        cv::Mat bgr;
        if (!cap.read(bgr) || bgr.empty()) {
            std::cerr << "[FrameExtract] Read first frame failed." << "\n";
            return 0;
        }

        char name[64];
        std::snprintf(name, sizeof(name), "%s%06d.jpg", filename_prefix.c_str(), 0);
        const auto out_path = (std::filesystem::path(out_dir) / name).string();

        std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, std::clamp(jpeg_quality, 1, 100) };
        if (!cv::imwrite(out_path, bgr, params)) {
            std::cerr << "[FrameExtract] Write failed: " << out_path << "\n";
            return 0;
        }

        std::cout << "[FrameExtract] Saved first frame to: " << out_path << "\n";
        return 1;
    };

    size_t n = extractFirstFrame(videoPath, outDir, quality, prefix);
    std::cout << "Saved " << n << " frame(s) to " << outDir << "\n";
    return 0;
}
