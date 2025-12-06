#include "vision/OrtYolo.h"
#include <random>
#include <vector>
#include <filesystem>

namespace vision {
    // OrtYoloDetector initializor
    OrtYoloDetector::OrtYoloDetector(const SessionOptions& opt)  // note: & opt temp var for transfering data only effective during construction
        : opt_(opt),                                    // init field opt_: opt
          env_(ORT_LOGGING_LEVEL_WARNING, "YOLOv8n"),   // init field env_: log level: warning, env name: YOLOv8n
          session_options_()                            // init field session options
    {
        /*  在此初始化 ONNX Runtime 会话
            onnxruntime will only be responsible for loading the .onnx files.
            the model should be loaded from ./data/models/yolov8n_640.onnx as default
        */
        if (opt_.fake_infer) {
            ready_ = true;
            return;
        }

        session_options_.SetIntraOpNumThreads(0);   // 0 = auto decide threads usage
        
        // load model file: convert path to wide string for Windows ONNX Runtime API
        std::wstring model_path_w(opt_.model_path.begin(), opt_.model_path.end());
        
        // create session instance (managed by unique_ptr, auto-destroyed with object)
        try {
            session_ = std::make_unique<Ort::Session>(env_, model_path_w.c_str(), session_options_);
            ready_ = true;
            std::cout << "[OrtYoloDetector] ONNX session created successfully with model: " << opt_.model_path << "\n"
                      << "                  Single multiclass model infer mode: " << opt_.use_single_multiclass_model << "\n                  (line 32)\n";
        } catch (const std::exception& ex) {
            std::cerr << "[OrtYoloDetector] Failed to create ONNX session: " << ex.what() << "\n                  (line 34)";
            ready_ = false; // remain not ready; infer() will return empty
        }
    }

    // OrtYoloDetector destructor: implemented in header

    bool const OrtYoloDetector::isReady() { return ready_; }

    std::vector<RawDet> OrtYoloDetector::infer(const cv::Mat& resized_rgb) {
        // ========= fake infer: 随机生成 0~2 个检测框 ===========
        if (opt_.fake_infer) {
            static std::mt19937 gen{123};
            std::uniform_real_distribution<float> uf(0.f, 1.f);
            std::vector<RawDet> dets;

            if (uf(gen) > 0.4f) { // person
                dets.push_back({uf(gen)*opt_.input_w, uf(gen)*opt_.input_h, 80.f, 120.f, 0.82f, 0});
            }
            if (uf(gen) > 0.7f) { // object
                dets.push_back({uf(gen)*opt_.input_w, uf(gen)*opt_.input_h, 60.f, 40.f, 0.63f, 1});
            }
            return dets;
        }

        // ========= check ready ===========
        std::cout << "[OrtYoloDetector] Checking if session is ready. (line 60)\n";

        if (!session_ || !OrtYoloDetector::isReady()) return {};
        
        // ========= real infer 流程框架 ===========
        // 1/ Get I/O node info (name, shape=[batch, channels, heights, widths])
        Ort::AllocatorWithDefaultOptions allocator;         // use allocator to derive the 
        
        std::cout << "[OrtYoloDetector] Preparing input and output node info. (line 69)\n";

        //      attain input node info ("images", [1, 3, 640, 640])
        Ort::AllocatedStringPtr input_name_ptr = session_->GetInputNameAllocated(0, allocator);
        const char* input_name = input_name_ptr.get();      // "images"
        Ort::TypeInfo input_type_info = session_->GetInputTypeInfo(0);
        auto input_tensor_shape = input_type_info.GetTensorTypeAndShapeInfo().GetShape();
        
        std::cout << "[OrtYoloDetector] Input tensor shape: [";
        for (size_t i = 0; i < input_tensor_shape.size(); ++i) {
            std::cout << input_tensor_shape[i];
            if (i < input_tensor_shape.size() - 1) std::cout << ", ";
        }
        std::cout << "]\n";

        //      attain output node info ("output0", [1, 84, 8400])
        Ort::AllocatedStringPtr output_name_ptr = session_->GetOutputNameAllocated(0, allocator);
        const char* output_name = output_name_ptr.get();    // "output0"
        
        // 2/ Preprocess (bgr->rgb, hwc([h,w,3])->nchw(r,g,b), normalize)
        if (resized_rgb.empty() || resized_rgb.cols != opt_.input_w || resized_rgb.rows != opt_.input_h) {
            std::cout << "[OrtYoloDetector] src/vision/OrtYolo.cpp: Input image size mismatch. \n  Expected "
                 << "width " << opt_.input_w << " and height " << opt_.input_h << ", got width "  // expected 640*640
                 << resized_rgb.cols << " and height " << resized_rgb.rows << "." << std::endl;
            return {};
        }

        std::cout << "[OrtYoloDetector] Preprocessing input image. (line 96)\n";

        cv::Mat rgb;
        cv::cvtColor(resized_rgb, rgb, cv::COLOR_BGR2RGB);  // BGR → RGB
        
        std::vector<float> input_tensor_val(1 * 3 * opt_.input_w * opt_.input_h);
        for (int c = 0; c < 3; ++c) {
            for (int h = 0; h < opt_.input_h; ++h) {
                for (int w = 0; w < opt_.input_w; ++w) {
                    int idx = c * opt_.input_h * opt_.input_w + h * opt_.input_w + w;
                    input_tensor_val[idx] = rgb.at<cv::Vec3b>(h, w)[c] / 255.0f;  // channel range from [0, 255] normalized to [0,1]
                }   // Vec3b: 3 elements vector
            } // take the c channel of the (h, w) pixel of rgb
        }

        // 3/   input tensor prepare
        std::vector<int64_t> input_shape = {1, 3, opt_.input_h, opt_.input_w};
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor_val.data(),    // float* p_data
            input_tensor_val.size(),    // size_t p_data_element_count
            input_shape.data(),         // int64_t* shape
            input_shape.size()          // size_t shape_len
        );

        std::cout << "[OrtYoloDetector] Running inference... (line 122)\n";

        // 4/ Inference run
        std::vector<const char*> input_names = {input_name};
        std::vector<const char*> output_names = {output_name};
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names.data(),  &input_tensor, 1,  // input name, tensor, cnt
            output_names.data(), 1                  // output count, cnt
        );

        std::cout << "[OrtYoloDetector] Inference completed. Processing output tensors. (line 133)\n";

        // 5/ Analysis output tensors
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();

        int num_boxes = static_cast<int>(output_shape.size()>=3 ? output_shape[2] : 0);
        int num_attrs = static_cast<int>(output_shape.size()>=2 ? output_shape[1] : 0);

        std::cout << "[OrtYoloDetector] Number of boxes: " << num_boxes << ", Number of attributes: " << num_attrs << " (infer line 145) \n";

        std::vector<RawDet> detect_results;
        const float conf_threshold = 0.25f;

        std::cout << "[OrtYoloDetector] Postprocessing output to extract detections. (line 150)\n";

        // 6/ Postprocessing
        if (num_attrs == 5) {          // Single-class model: attrs-first [cx,cy,w,h,conf]

            // report using A single class for person detection
            std::cout << "[OrtYoloDetector] Using single-class person detection postprocessing. (line 156)\n";
            
            for (int i = 0; i < num_boxes; ++i) {
                float cx = output_data[i];
                float cy = output_data[num_boxes + i];
                float w  = output_data[2 * num_boxes + i];
                float h  = output_data[3 * num_boxes + i];
                float conf = output_data[4 * num_boxes + i];
                if (conf >= conf_threshold) {
                    RawDet det{cx, cy, w, h, conf, /*cls_id*/ 0}; // 0 约定为 person
                    detect_results.push_back(det);
                }
            }
        } else if (num_attrs >= 6) {   // Multi-class model: attrs-last [cx,cy,w,h] + num_classes (allow objness)

            // report using B multi-class detection
            std::cout << "[OrtYoloDetector] Using multi-class detection postprocessing.(line 172)\n";
            
            int num_classes = num_attrs - 4;
            bool has_objness = false;
            int cls_offset = 4;
            if (num_attrs == 85) { has_objness = true; cls_offset = 5; num_classes = 80; }
            if (num_attrs == 14) { has_objness = false; cls_offset = 4; num_classes = 10; } // custom 10-class model with objness
            // 如果是 84，则 objness 合并到类分数；若是 85，则第 5 行是 objness
            for (int i = 0; i < num_boxes; ++i) {
                float cx = output_data[i];
                float cy = output_data[num_boxes + i];
                float w  = output_data[2 * num_boxes + i];
                float h  = output_data[3 * num_boxes + i];
                float obj = has_objness ? output_data[4 * num_boxes + i] : 1.0f;
                float best_score = 0.f; int best_cls = -1;
                for (int c = 0; c < num_classes; ++c) {
                    float score = output_data[(cls_offset + c) * num_boxes + i];
                    if (has_objness) score *= obj;
                    if (score > best_score) { best_score = score; best_cls = c; }
                }
                if (best_score >= conf_threshold) {
                    RawDet det{cx, cy, w, h, best_score, best_cls};
                    detect_results.push_back(det);
                }
            }
        } else {
            std::cerr << "[OrtYoloDetector] Unexpected attributes count: " << num_attrs << " (line 198)\n";
        }

        std::cout << "[OrtYoloDetector] Total detections after filtering: " << detect_results.size() << " (line 201)\n";

        // single multi-class model: early return
        if (opt_.use_single_multiclass_model) {
            return detect_results;
        }
 
        // ================= Two Models Inference (Person & Object) =================

        static std::unique_ptr<Ort::Env> obj_env;
        static std::unique_ptr<Ort::SessionOptions> obj_opts;
        static std::unique_ptr<Ort::Session> obj_sess;
        
        static const std::string obj_model_path_a = "../../assets/vision/weights/fine_tune02.onnx";
        static const std::string obj_model_path_b = "../../assets/vision/weights/yolov8n_640.onnx";
        const std::string obj_model_path = std::filesystem::exists(obj_model_path_a) ? obj_model_path_a : obj_model_path_b;

        if (std::filesystem::exists(obj_model_path)) {
            if (!obj_env) {obj_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YOLOv8n-obj"); std::cerr << "[OrtYoloDetector] Object model environment created as \"" << obj_model_path << "\". (line 216)\n"; };
            if (!obj_opts) {obj_opts = std::make_unique<Ort::SessionOptions>(); obj_opts->SetIntraOpNumThreads(1); std::cerr << "[OrtYoloDetector] Object model session options created as \"" << obj_model_path << "\". (line 217)\n"; };
            if (!obj_sess) {
                try {
                    std::wstring obj_model_w(obj_model_path.begin(), obj_model_path.end());
                    obj_sess = std::make_unique<Ort::Session>(*obj_env, obj_model_w.c_str(), *obj_opts);
                    std::cerr << "[OrtYoloDetector] Object model session created as \"" << obj_model_path << "\". (line 222)\n";
                } catch (const std::exception& ex) {
                    std::cerr << "[OrtYoloDetector] Failed to create object session: " << ex.what() << " (line 224)\n";
                }
            }

            // 若 物品模型 会话构造成功：二次推理
            if (obj_sess) {

                // report construction of the object session
                std::cout << "[OrtYoloDetector] Object model session loaded from: " << obj_model_path << " (line 232)\n";

                // 复用已构造的 input_tensor
                Ort::AllocatedStringPtr obj_input_name_ptr = obj_sess->GetInputNameAllocated(0, allocator);
                const char* obj_input_name = obj_input_name_ptr.get();
                Ort::AllocatedStringPtr obj_output_name_ptr = obj_sess->GetOutputNameAllocated(0, allocator);
                const char* obj_output_name = obj_output_name_ptr.get();

                std::cout << "[OrtYoloDetector] Running inference on object model... (line 240)\n";

                // Run inference of object model
                std::vector<const char*> obj_in_names = {obj_input_name};
                std::vector<const char*> obj_out_names = {obj_output_name};
                auto obj_outs = obj_sess->Run(Ort::RunOptions{nullptr}, obj_in_names.data(), &input_tensor, 1, obj_out_names.data(), 1);

                std::cout << "[OrtYoloDetector] Object model inference completed. Processing output tensors. (line 247)\n";


                float* obj_data = obj_outs[0].GetTensorMutableData<float>();
                auto obj_shape = obj_outs[0].GetTensorTypeAndShapeInfo().GetShape();
                int obj_boxes = static_cast<int>(obj_shape.size()>=3 ? obj_shape[2] : 0);
                int obj_attrs = static_cast<int>(obj_shape.size()>=2 ? obj_shape[1] : 0);

                // report output shape of the object model
                std::cout << "[OrtYoloDetector] Object model output tensor shape: [" << obj_shape[0] << ", " << obj_shape[1] << ", " << obj_shape[2] << "] (line 256)\n";
                std::cout << "[OrtYoloDetector] Number of boxes: " << obj_boxes << ", Number of attributes: " << obj_attrs << " (object) (line 257)\n";

                // 默认按 YOLOv8 COCO 80 类处理
                int obj_num_classes = std::max(0, obj_attrs - 4);
                bool obj_has_objness = (obj_attrs == 85);
                int obj_cls_off = obj_has_objness ? 5 : 4;
                for (int i = 0; i < obj_boxes; ++i) {
                    float cx = obj_data[i];
                    float cy = obj_data[obj_boxes + i];
                    float w  = obj_data[2 * obj_boxes + i];
                    float h  = obj_data[3 * obj_boxes + i];
                    float objn = obj_has_objness ? obj_data[4 * obj_boxes + i] : 1.0f;
                    float best_s = 0.f; int best_c = -1;
                    for (int c = 0; c < obj_num_classes; ++c) {
                        float sc = obj_data[(obj_cls_off + c) * obj_boxes + i];
                        if (obj_has_objness) sc *= objn;
                        if (sc > best_s) { best_s = sc; best_c = c; }
                    }
                    if (best_s >= conf_threshold) {
                        RawDet det{cx, cy, w, h, best_s, best_c};
                        detect_results.push_back(det);
                    }
                }

                std::cout << "[OrtYoloDetector] Total detections after merging object model: " << detect_results.size() << "\n";
            }
        }

        return detect_results;
    }
} // namespace vision
