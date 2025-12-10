#ifndef SEAT_STATE_JUDGER_HPP
#define SEAT_STATE_JUDGER_HPP

#include "data_structures.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>


class SeatDatabase; // forward declare

#include <json.hpp> 

using json = nlohmann::json;
using namespace cv;
using namespace std;

const float IOU_THRESHOLD = 0.3f;
const float FG_RATIO_THRESHOLD = 0.05f;
const int ANOMALY_THRESHOLD_SECONDS = 6;
const int MORPH_KERNEL_SIZE = 5;

class SeatStateJudger {
public:
    SeatStateJudger();
    ~SeatStateJudger() = default;

    
    void processAData(
        const struct A2B_Data& a_data,
        const json& seat_j,
        struct B2CD_State& state,
        vector<struct B2CD_Alert>& alerts,
        struct B2C_SeatSnapshot& out_snapshot,
        optional<struct B2C_SeatEvent>& out_event
    );

    bool readLastFrameData(
        vector<struct A2B_Data>& out_a2b_data_list,
        vector<json>& out_seat_j_list
    );

    void run(const std::string& jsonl_path = "");
    string stateToStr(int status_enum); // accepts B2CD_State::SeatStatus(int)
    string msToISO8601(int64_t ts_ms);
    bool readJsonlFile(
        const string& jsonl_path,
        vector<vector<A2B_Data>>& out_batch_a2b_data,
        vector<vector<json>>& out_batch_seat_j
    );

    vector<int> getNeedStoreFrameIndexes() const {
        return vector<int>(need_store_frame_indexes_.begin(), need_store_frame_indexes_.end());
    }

    void resetNeedStoreFrameIndexes() {
        need_store_frame_indexes_.clear();
    }

private:
    // state tracking
    unordered_map<string, int64_t> last_seat_ts_;
    unordered_map<string, int> last_seat_status_duration_;
    unordered_map<string, int> anomaly_occupied_duration_;
    unordered_map<string, int> last_seat_status_; // store as int (enum value)

    unordered_set<int> need_store_frame_indexes_;

    // DB reference
    SeatDatabase* db_; // pointer to avoid ctor-order issues

    
    // tracking seat status durations
    class SeatTimer {
    public:
        bool is_running = false;
        chrono::steady_clock::time_point start_time;
        int getElapsedSeconds();
    };
    unordered_map<string, SeatTimer> seat_timers_;

    float calculateIoU(const Rect& rect1, const Rect& rect2);
    string getISO8601Timestamp();
    std::unordered_set<std::string> processed_files_;
};

#endif 
