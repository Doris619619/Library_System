// Minimal perf test executable entry point.
#include "vision/VisionA.h"
#include "vision/Publish.h"
#include "vision/Config.h"
#include "vision/Types.h"
#include "vision/FrameProcessor.h"
#include "vision/VisionClient.h"

#include <opencv2/opencv.hpp>
#include <filesystem>
#include <string>
#include <algorithm>
#include <set>
#include <iostream>
#include <chrono>
#include <fstream>
#include <cstddef>

int main() {
	auto start = std::chrono::high_resolution_clock::now();
	// TODO: add performance measurement invoking Vision components.
	auto end = std::chrono::high_resolution_clock::now();
	std::cout << "[Test Performance] test_perf stub elapsed ms: "
			  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
			  << "\n";
	
	vision::VisionClient vc;
	vc.runVision(
		"./assets/vision/videos/demo.mp4", 
		"./out/000000.jsonl", 
		"10", 
		"0.5", 
		"true"
	);
	
	return 0;
}
