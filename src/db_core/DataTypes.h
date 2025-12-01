#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <string>
#include <vector>
#include <map>

// 座位状态结构 - 对应B模块的B2CD_State
struct SeatStatus {
    std::string seat_id;
    std::string state;  // "Seated", "Unseated", "Anomaly"
    std::string last_update; // 标准时间戳格式
    
    SeatStatus(const std::string& id = "", const std::string& s = "Unseated", const std::string& update = "")
        : seat_id(id), state(s), last_update(update) {}
};

// 基础统计信息 - 对应UI显示需求
struct BasicStats {
    int total_seats;
    int occupied_seats;
    int anomaly_seats;
    double overall_occupancy_rate;
    
    BasicStats() : total_seats(0), occupied_seats(0), anomaly_seats(0), overall_occupancy_rate(0.0) {}
};

// 小时数据 - 用于趋势分析
struct HourlyData {
    std::string hour;
    double occupancy_rate;
    
    HourlyData(const std::string& h = "", double rate = 0.0) : hour(h), occupancy_rate(rate) {}
};

// 对应B模块的B2CD_Alert
struct AlertData {
    std::string alert_id;
    std::string seat_id;
    std::string alert_type;  // "AnomalyOccupied"等
    std::string alert_desc;
    std::string timestamp;
    bool is_processed;
};

// 对应B模块的B2C_SeatSnapshot
struct SeatSnapshot {
    std::string seat_id;
    std::string state;
    int person_count;
    std::string timestamp;
};

// 对应B模块的B2C_SeatEvent
struct SeatEvent {
    std::string seat_id;
    std::string state;
    std::string timestamp;
    int duration_sec;
};

#endif // DATA_TYPES_H
