#pragma once
#include "./third_party/onnxruntime/include/onnxruntime_cxx_api.h"
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace vision {

    struct RawDet {         // 原始检测框数据结构(model output)
        float cx, cy, w, h;
        float conf;
        int cls_id;
    };

    class OrtYoloDetector {
    public:
        struct SessionOptions {
            std::string model_path = "assets/weights/fine_tune02.onnx";//"assets/weights/person01.onnx";
            int input_w = 640;
            int input_h = 640;
            bool fake_infer = true;
            bool use_single_multiclass_model = true; // switch of only using one single multi-class model
        };

        explicit OrtYoloDetector(const SessionOptions& opt);
        ~OrtYoloDetector() = default;
        bool const isReady();
        std::vector<RawDet> infer(const cv::Mat& resized_rgb); // resized 640x640

    private:
        SessionOptions opt_;
        bool ready_ = false;

        // onnx runtime session
        Ort::Env env_;                          // env object
        Ort::SessionOptions session_options_;   // session options config
        std::unique_ptr<Ort::Session> session_; // session instance
    };

} // namespace vision
