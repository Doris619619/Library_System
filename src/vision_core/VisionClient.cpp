#include "seatui/vision/VisionA.h"
#include "seatui/vision/Publish.h"
#include "seatui/vision/Config.h"
#include "seatui/vision/Types.h"
#include "seatui/vision/FrameProcessor.h"
#include "seatui/vision/VisionClient.h"

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
    VisionClient::VisionClient() 
    {
        // Implementation of VisionClient constructor
        // Initialize VisionA and FrameProcessor objects
        VisionConfig cfg;
        VisionA vision(cfg);
        FrameProcessor fp;
    }

    //     /* TODO: 
    //     *  1. 对象构建（VisionConfig cfg, FrameProcessor fp）（要将impl内的东西传到方法内部(设想通过struct另外打包传入)）
    //     *  2. 输入解析；
    //     *  3. 基本参数设定；
    //     *  4. 根据分类调用方法进行处理
    //     *  5. 输出结果
    //     *  
    //     *  Notes: 现在不需要实现回传，功能不要求——不需要回传与照片绘制，可以先不实现
    //     *  //6. 等待B回传的入库帧，接收 
    //     *  //7. 找回对应帧，按需绘制（默认不需）
    //     *  //8. 保存入库（入库路径放到？：）
    //     */

    // 运行VisionClient
    int VisionClient::runVision(
        const std::string& frame_src,                   // frame / video src
        const std::string& output_file,                 // output file default as ./out/xxxxxx.jsonl
        const std::string& max_process_frames_string,   // max process frames default as 500
        const std::string& sample_fps,                  // fps sampling default as one frame per 2 seconds, requires video fps to judge, need extra judging method and mechanism
        const std::string& is_stream_process            // stream process or not, default use stream process
    ) {
        // Implementation of runVision method
        // This method would handle the processing logic

        // CLI args settings (parts)
        std::string img_dir;                               // --framesrc 输入图像目录或视频文件
        std::string override_out_states;                   // --out 覆盖输出文件
        size_t max_process_frames = static_cast<size_t>(std::stoll(max_process_frames_string));              // --max 处理帧数上限
        double extract_fps = 0.5;                          // --fps 视频抽帧频率
        bool stream_video = true;                         // --stream false: 直接逐帧, 不落盘抽帧目录; true: 抽帧落盘后处理
        std::set<std::string> on_bag = {"1", "true", "True", "t", "T", "yes", "y", "on", "On", "ON", "v", "V"};
        std::set<std::string> off_bag = {"0", "false", "False", "f", "F", "no", "n", "off", "Off", "OFF", "x", "X"};
        
        VisionConfig cfg = VisionConfig::fromYaml("config/vision.yml");
        VisionA vision(cfg);

        // anal args 实际使用输入参数
        try { img_dir = frame_src; } catch (...) { std::cerr << "[VisionClient] Error in arg (input file / directory) identification\n"; img_dir = "./assests/vision/videos/demo.mp4"; }                             // 输入路径为图像目录
        //img_dir = frame_src;
        try { override_out_states = output_file; } catch (...) { std::cerr << "[VisionClient] Error in arg output_file\n"; override_out_states = "./out/000000.jsonl"; }
        //override_out_states = output_file;
        try { max_process_frames = static_cast<size_t>(std::stoll(max_process_frames_string)); } catch (...) { std::cerr << "[VisionClient] Error in arg max_process_frames_string\n"; max_process_frames = 500;}
        //max_process_frames = static_cast<size_t>(std::stoll(max_process_frames_string));
        try { extract_fps = std::stod(sample_fps); } catch (...) { std::cerr << "[VisionClient] Error in arg sample_fps\n"; extract_fps = 0.5; }
        //extract_fps = static_cast<double>(std::stod(sample_fps));
        try { stream_video = (off_bag.count(is_stream_process) > 0) ? false : (on_bag.count(is_stream_process) > 0 ? true : true); } catch (...) { std::cerr << "[VisionClient] Error in arg is_stream_video\n" << "\n"; stream_video = true; }

        std::cout << "[VisionClient] Demo starting...\n";
        std::cout << "[VisionClient] CWD: " << std::filesystem::current_path().string() << "\n";         // current working directory
        std::cout << "[VisionClient] Input: " << img_dir << "\n";
        std::cout << "[VisionClient] Output states file: " << (override_out_states.empty() ? "None" : override_out_states) << "\n";
        std::cout << "[VisionClient] Options: max=" << (max_process_frames==SIZE_MAX? -1 : (long long)max_process_frames) << "\n"
                  << "                fps=" << extract_fps << "\n"
                  << "                stream=" << (stream_video ? true : false) << "\n";
        std::cout.flush();

        // Load config with internal exception-safety (fromYaml keeps defaults on failure)
        if (!std::filesystem::exists("./assets/vision/config/vision.yml")) {  // yaml existence
            std::cerr << "[VisionClient] ./assets/vision/config/vision.yml not found relative to CWD.\n";
            return 1;
        }
        if (!std::filesystem::exists(cfg.seats_json)) {       // seat json existence
            std::cerr << "[VisionClient] seats json not found: " << cfg.seats_json << "\n";
            if (!std::filesystem::create_directory("./assets/vision/config/demo_seats.json")) {
                std::cerr << "[VisionClient] Failed to create directory for demo seats.json at ./assets/vision/config/demo_seats.json. \n"
                          << "               Hint: make sure the seats.json file exists and the path is correct.\n";
                return 1;
            }
        }
        if (!std::filesystem::exists(img_dir)) {              // input path existence
            std::cerr << "[VisionClient] Input path not found: " << img_dir << "\n";
            std::cerr << "               Hint: use a directory of images or a video file path. Default as ./assets/vision/videos/demo.mp4 \n";
            return 1;
        }
    
        // 使用配置中的 states_output 输出座位状态记录(runtime/seat_states.jsonl)，若用户cli提供则覆盖
        std::string out_states_path = (override_out_states.empty() || override_out_states != "./out/000000.jsonl") ? cfg.states_output : override_out_states;
        std::cout << "[VisionClient] Output states file: " << out_states_path << "\n";
        std::cout << "[VisionClient] Loaded seats from " << cfg.seats_json 
                  << ": count=" << vision.seatCount() 
                  << "\n";
    
        // Publish part
        Publisher pub;
        pub.setCallback([](const std::vector<SeatFrameState>& states){
            std::cout << "[VisionClient] Callback batch size = " << states.size() << "\n";
        });
        vision.setPublisher(&pub);

        // create output path (skip if no parent path included)
        int64_t frame_index = 0;                                             // index of img handled this patch
        auto output_state_parent_path = std::filesystem::path(out_states_path).parent_path();  // parent path of the output file

        if (output_state_parent_path.empty()) {     
            std::error_code error_code_;
            std::filesystem::create_directories(output_state_parent_path, error_code_);
            if (error_code_) {
                std::cerr << "[VisionClient] Failed to create output state directory: " << output_state_parent_path.string() << " : " << error_code_.message() << "\n"
                          << "       Hint: check if the path is valid. By default the path for saving output states is ./runtime/seat_states.jsonl \n";
                return 1;
            }
        }
    
        // open output states file (append mode)
        std::ofstream ofs(out_states_path, std::ios::app);

        //if (!ofs) {                     // error check: open output states file
        //    std::cerr << "[VisionClient] Failed to open output states file: " << out_states_path << "\n";
        //    return 1;
        //}
        //std::cout << "[VisionClient] States output file: " << std::filesystem::absolute(out_states_path).string() << "\n";
    
        // frame annotated directory
        std::error_code error_code_marker;
        std::filesystem::create_directories(cfg.annotated_frames_dir, error_code_marker);

        // judge input type
        std::string input_type_str;
        switch (vision::FrameProcessor::judgeInputType(img_dir)) {
            case vision::InputType::DIRECTORY_IMAGE:
                input_type_str = "IMAGE";
                break;
            case vision::InputType::VIDEO_FILE:
                input_type_str = "VIDEO";
                break;
            case vision::InputType::IMAGE_FILE:
                input_type_str = "IMAGE";
                break;
            case vision::InputType::NOT_EXISTS:
                std::cerr << "[VisionClient] Input path not exists: " << img_dir << "\n";
                return 1;
            case vision::InputType::UNKNOWN:
                input_type_str = "UNKNOWN";
                std::cerr << "[VisionClient] Input path type UNKNOWN: " << img_dir << "\n"
                          << "       Maybe not a supported format. .jpg/.png are recommendde for images; .mp4 for video.\n"
                          << "       Hint: use a directory of images or a video file path.\n";
                return 1;
                break;
            default:
                input_type_str = "UNKNOWN";
                break;
        }
        std::cout << "[VisionClient] Input type identified as: " << input_type_str << "\n";

        // Formal process
        size_t total_processed = 0;
        size_t total_errors = 0;
        int print_every = 100; // 进度打印间隔
        int annotated_save_cnt = 0;
        int annotated_save_freq = cfg.annotated_save_freq;
    
        try {    // Extract and Process   
            auto input_path = std::filesystem::path(img_dir); // here the input_path can be video file or directory of img
            auto input_path_string = input_path.string();

            // criterion cannot use "is_directory": video can be identified as directory

            // judging images or video to be processed
        
            // Images Processing (Directly, NO Extraction)
            if (input_type_str == "IMAGE") {  // process frame from img directory

            // conduct processing via imageProcess
                size_t processed = vision::FrameProcessor::imageProcess(
                    input_path_string,                  // images directory path string
                    output_state_parent_path.string(),  // output states parent path
                    ofs,                                // output file stream
                    cfg,                                // VisionConfig
                    vision,                             // VisionA
                    max_process_frames,                 // max process frames
                    50,                                 // frames to sample per 100 images
                    0                                   // original total index offset (will recheck cnt of all images in directory if 0 provided)
                );

            // output processed frames cnt
                std::cout << "[VisionClient] Processed image frames: " << processed << "\n";

            // publish record

            // receive frame idx to be annotated

            // annotation

            // publish annotation

            } else if (input_type_str == "VIDEO") {// Video Processing
            
                std::cout << "[VisionClient] Input is a video file, " << (stream_video ? "streaming frames..." : "extracting frames then processing...") << "\n";

                // Streaming Process video: Extract and Process Frame-by-Frame from Video
                if (stream_video) { 

                    // iterate over the video to extract & process frames 1-by-1
                    size_t processed = vision::FrameProcessor::streamProcess(
                        input_path_string,                 // video path
                        output_state_parent_path.string(), // output states parent path                        
                        vision,                            // VisionA
                        cfg,                               // VisionConfig
                        ofs,                               // output file stream
                        extract_fps,                       // extract fps
                        0,                                 // start frame idx
                        -1,                                // ending frame idx (-1: all frames) 
                        max_process_frames                 // maximum frames to process
                    );

                    // output processed frames cnt
                    std::cout << "[VisionClient] Processed video frames: " << processed << "\n";

                    // publish record

                    // receive frame idx to be annotated

                    // annotation

                    // publish annotation
            
                } else {  /* Bulk Process Video: Extract all sampled frames to directory then process them */

                    // extract all out then process
                    size_t processed = vision::FrameProcessor::bulkProcess(
                        input_path_string,                        // video_path
                        output_state_parent_path.string(),        // latest_frame_dir (for last_frame.json)
                        cfg,                                      // VisionConfig
                        ofs,                                      // output file stream
                        vision,                                   // VisionA
                        extract_fps,                              // sample_fps
                        0,                                        // start_frame_idx
                        -1,                                       // end_frame_idx (-1: all frames)
                        std::string("./assets/vision/extracted/"),// img_dir to store extracted frames
                        max_process_frames,                       // max_process_frames
                        95,                                       // jpg_quality
                        "f_"                                      // filename_prefix
                    );

                    // output processed frames cnt
                    std::cout << "[VisionClient] Processed video frames: " << processed << "\n";

                    // publish record

                    // receive frame idx to be annotated

                    // annotation

                    // publish annotation

                }
            } else {
                std::cerr << "[VisionClient] Unsupported input type for path: " << img_dir << "\n";
                return 1;
            }
              
        } catch (const std::filesystem::filesystem_error& filesystem_error) {  // Filesystem exception
            std::cerr << "[VisionClient] Filesystem error: " << filesystem_error.what() << "\n";
            return 1;
        } catch (const std::exception &exception) { // General exception
            std::cerr << "[VisionClient] Unhandled exception: " << exception.what() << "\n";
            return 1;
        } catch (...) {  // Unknow exception
            std::cerr << "[VisionClient] Unhandled unknown exception.\n";
            return 1;
        }
        std::cout << "[VisionClient] Seat states appended to: " << out_states_path << "\n";
        std::cout << "[VisionClient] Summary: processed=" << total_processed << " errors=" << total_errors << "\n";
        std::cout << "[VisionClient] Latest frame snapshot: " << output_state_parent_path.string() + "/last_frame.json" << "\n";
        return 0; 
    }

    // now_t_ms
    int64_t now_ms() { 
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

}


