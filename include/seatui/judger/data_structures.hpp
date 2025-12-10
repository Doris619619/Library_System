#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP
#pragma once
#include <opencv2/core.hpp>    
#include <opencv2/imgproc.hpp> 
#include <string>
#include <vector>
#include <optional>            

// get A's person_boxes/object_boxes
struct DetectedObject {
    std::string class_name;  // "person"/"object"
    cv::Rect bbox;           // x/y/w/h
    float score;             // conf
    int class_id;            // cls_id
};

// get A's A2B_Data structure
struct A2B_Data {
    int frame_id;                          // frame_index
    std::string seat_id;                   // seat_id
    cv::Rect seat_roi;                     // seat_roi
    std::vector<cv::Point2i> seat_poly;    // seat_poly
    std::vector<DetectedObject> person_boxes; 
    std::vector<DetectedObject> object_boxes; 
    std::string timestamp;                 // ts_ms
    cv::Mat frame;                         // image_path
};

// B's state to C/D
struct B2CD_State {
    std::string seat_id;                   
    enum SeatStatus {                      
        UNSEATED = 0,
        SEATED = 1,
        ANOMALY_OCCUPIED = 2
    } status;
    int status_duration;               
    float confidence;                      
    std::string timestamp;                 
    int source_frame_id;                  
};


// B's alert to C/D
struct B2CD_Alert {
    std::string alert_id;                  // seat_id_timestamp
    std::string seat_id;                   
    std::string alert_type;                // AnomalyOccupied
    std::string alert_desc;                // description
    std::string timestamp;                 
    bool is_processed = false;             
};

// B's event to C
struct B2C_SeatEvent {
    std::string seat_id;          
    std::string state;            
    std::string timestamp;        
    int duration_sec;             
};

// B's snapshot to C
struct B2C_SeatSnapshot {
    std::string seat_id;          
    std::string state;            
    int person_count;             
    std::string timestamp;        
};

#endif 