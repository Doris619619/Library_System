#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <string>
#include <vector>
#include <map>

//Seat Status Structure - Corresponding to B2CD_State of Module B
struct SeatStatus {
    std::string seat_id;
    std::string state;  // "Seated", "Unseated", "Anomaly"
    std::string last_update; // Standard timestamp format
    
    SeatStatus(const std::string& id = "", const std::string& s = "Unseated", const std::string& update = "")
        : seat_id(id), state(s), last_update(update) {}
};

// B2CD_Alert corresponding to Module B
struct AlertData {
    std::string alert_id;
    std::string seat_id;
    std::string alert_type;  // "AnomalyOccupied"等
    std::string alert_desc;
    std::string timestamp;
    bool is_processed;
};

// Corresponding B2C_SeatSnapshot of Module B
struct SeatSnapshot {
    std::string seat_id;
    std::string state;
    int person_count;
    std::string timestamp;
};

// Corresponding B2C_SeatEvent of Module B
struct SeatEvent {
    std::string seat_id;
    std::string state;
    std::string timestamp;
    int duration_sec;
};

// Basic Statistical Information - Corresponding UI Display Requirements
struct BasicStats {
    int total_seats;
    int occupied_seats;
    int anomaly_seats;
    double overall_occupancy_rate;
    
    BasicStats() : total_seats(0), occupied_seats(0), anomaly_seats(0), overall_occupancy_rate(0.0) {}
};

// Hourly Data - For Trend Analysis
struct HourlyData {
    std::string hour;
    double occupancy_rate;
    
    HourlyData(const std::string& h = "", double rate = 0.0) : hour(h), occupancy_rate(rate) {}
};

#endif // DATA_TYPES_H
