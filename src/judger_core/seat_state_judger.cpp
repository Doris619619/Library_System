#include "seatui/judger/seat_state_judger.hpp"
#include "../db_core/SeatDatabase.h"
#include "../db_core/DatabaseInitializer.h"  

#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <thread>
#include <climits>

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace std;
using namespace cv;

static inline std::string normalizeSeatId(const std::string& raw) {
    if (raw.empty()) return "S0";
    if (raw[0] == 'S') return raw;
    return "S" + raw;
}


SeatStateJudger::SeatStateJudger()
{
    try {
        db_ = &SeatDatabase::getInstance();
        db_->initialize();

        // insert  initialized seat data
        DatabaseInitializer dbInit(*db_);  // pass in database reference
        bool success = dbInit.insertSampleSeats();
        if (success) {
            std::cout << "[B] Successfully inserted sample seats into database." << std::endl;
        } else {
            std::cout << "[B] Failed to insert sample seats." << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "[B] SeatDatabase initialization failed " << e.what() << std::endl;
        db_ = nullptr;
    }
}

int SeatStateJudger::SeatTimer::getElapsedSeconds() {
    if (!is_running) return 0;
    auto now = chrono::steady_clock::now();
    auto elapsed = chrono::duration_cast<chrono::seconds>(now - start_time);
    return static_cast<int>(elapsed.count());
}

string SeatStateJudger::getISO8601Timestamp() {
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    struct tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &in_time_t);
#else
    localtime_r(&in_time_t, &local_tm);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    return string(buf);
}

string SeatStateJudger::msToISO8601(int64_t ts_ms) {
    time_t sec = static_cast<time_t>(ts_ms / 1000);
    struct tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &sec);
#else
    localtime_r(&sec, &local_tm);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    return string(buf);
}

string SeatStateJudger::stateToStr(int status_enum) {
    if (status_enum == 1) return "Seated";
    if (status_enum == 2) return "Anomaly";
    return "Unseated";
}

float SeatStateJudger::calculateIoU(const Rect& rect1, const Rect& rect2) {
    int x1 = max(rect1.x, rect2.x);
    int y1 = max(rect1.y, rect2.y);
    int x2 = min(rect1.x + rect1.width, rect2.x + rect2.width);
    int y2 = min(rect1.y + rect1.height, rect2.y + rect2.height);
    int inter_w = max(0, x2 - x1);
    int inter_h = max(0, y2 - y1);
    int inter_area = inter_w * inter_h;
    int area1 = rect1.width * rect1.height;
    int area2 = rect2.width * rect2.height;
    int union_area = area1 + area2 - inter_area;
    if (union_area == 0) return 0.0f;
    return static_cast<float>(inter_area) / static_cast<float>(union_area);
}


bool SeatStateJudger::readJsonlFile(
    const string& jsonl_path,
    vector<vector<A2B_Data>>& batch_a2b_data,
    vector<vector<json>>& batch_seat_j
) {
    batch_a2b_data.clear();
    batch_seat_j.clear();

    if (jsonl_path.empty()) {
        cout << "[B] Error: readJsonlFile got empty path" << endl;
        return false;
    }

    if (!fs::exists(jsonl_path) || !fs::is_regular_file(jsonl_path)) {
        cout << "[B] Error: JSONL file not found: " << jsonl_path << endl;
        return false;
    }

    ifstream file(jsonl_path);
    if (!file.is_open()) {
        cout << "[B] Error: Failed to open JSONL file: " << jsonl_path << endl;
        return false;
    }

    string line;
    int frame_count = 0;

    vector<A2B_Data> frame_a2b;
    vector<json> frame_seat_j;

    // json's current directory for relative paths
    fs::path json_dir = fs::path(jsonl_path).parent_path();

    while (getline(file, line)) {
        if (line.empty()) continue;
        try {
            json j = json::parse(line);

            int frame_index = j.value("frame_index", 0);
            string timestamp = msToISO8601(j.value("ts_ms", 0LL));

            if (!j.contains("seats") || !j["seats"].is_array()) continue;

            frame_a2b.clear();
            frame_seat_j.clear();

            for (auto& seat_j : j["seats"]) {
                A2B_Data a2b;
                a2b.frame_id = frame_index;
                a2b.timestamp = timestamp;
                a2b.frame = Mat(); 

                // seat_id
                if (seat_j.contains("seat_id")) {
                    if (seat_j["seat_id"].is_string()) a2b.seat_id = seat_j["seat_id"].get<string>();
                    else if (seat_j["seat_id"].is_number()) a2b.seat_id = to_string(seat_j["seat_id"].get<int>());
                    else a2b.seat_id = "unknown";
                } else a2b.seat_id = "unknown";

                // seat_roi
                int roi_x = seat_j.value("seat_roi", json::object()).value("x", 0);
                int roi_y = seat_j.value("seat_roi", json::object()).value("y", 0);
                int roi_w = seat_j.value("seat_roi", json::object()).value("w", 0);
                int roi_h = seat_j.value("seat_roi", json::object()).value("h", 0);
                if (roi_w <= 0 || roi_h <= 0) a2b.seat_roi = Rect(0,0,1,1);
                else a2b.seat_roi = Rect(roi_x, roi_y, roi_w, roi_h);

                // seat_poly
                a2b.seat_poly.clear();
                if (seat_j.contains("seat_poly") && seat_j["seat_poly"].is_array()) {
                    for (auto& p : seat_j["seat_poly"]) {
                        if (p.is_array() && p.size() == 2) {
                            a2b.seat_poly.emplace_back(p[0].get<int>(), p[1].get<int>());
                        }
                    }
                    if ((a2b.seat_roi.width == 1 && a2b.seat_roi.height == 1) && !a2b.seat_poly.empty()) {
                        a2b.seat_roi = boundingRect(a2b.seat_poly);
                    }
                }

                // person_boxes
                a2b.person_boxes.clear();
                if (seat_j.contains("person_boxes") && seat_j["person_boxes"].is_array()) {
                    for (auto& pb : seat_j["person_boxes"]) {
                        DetectedObject obj;
                        obj.bbox = Rect(pb.value("x",0), pb.value("y",0),
                                        pb.value("w",0), pb.value("h",0));
                        obj.score = static_cast<float>(pb.value("conf",0.0)) / 10.0f;
                        obj.class_name = pb.value("cls_name", string("person"));
                        obj.class_id = pb.value("cls_id", 0);
                        a2b.person_boxes.push_back(obj);
                    }
                }

                // object_boxes
                a2b.object_boxes.clear();
                if (seat_j.contains("object_boxes") && seat_j["object_boxes"].is_array()) {
                    for (auto& ob : seat_j["object_boxes"]) {
                        DetectedObject obj;
                        obj.bbox = Rect(ob.value("x",0), ob.value("y",0),
                                        ob.value("w",0), ob.value("h",0));
                        obj.score = static_cast<float>(ob.value("conf",0.0)) / 10.0f;
                        obj.class_name = ob.value("cls_name", string("object"));
                        obj.class_id = ob.value("cls_id", 0);
                        a2b.object_boxes.push_back(obj);
                    }
                }

                frame_a2b.push_back(a2b);
                frame_seat_j.push_back(seat_j);
            }

            if (!frame_a2b.empty()) {
                batch_a2b_data.push_back(frame_a2b);
                batch_seat_j.push_back(frame_seat_j);
                ++frame_count;
            }
        } catch (const json::exception& e) {
            cout << "[B] Error: Failed to parse JSONL line: " << e.what() << endl;
            continue;
        }
    }

    cout << "[B] Successfully read " << frame_count << " frames of batch data from " << jsonl_path << endl;
    return frame_count > 0;
}


void SeatStateJudger::processAData(
    const A2B_Data& a_data,
    const json& seat_j,
    B2CD_State& state,
    vector<B2CD_Alert>& alerts,
    B2C_SeatSnapshot& out_snapshot,
    optional<B2C_SeatEvent>& out_event
) {
    // initialize state
    state.seat_id = normalizeSeatId(a_data.seat_id);
    state.timestamp = a_data.timestamp;
    state.confidence = 0.90f;
    state.status_duration = 0;
    state.source_frame_id = a_data.frame_id;
    state.status = static_cast<B2CD_State::SeatStatus>(0); // UNSEATED

    int64_t current_ts_ms = seat_j.value("ts_ms", 0LL);

    // time difference calculation
    int time_diff_sec = 0;
    if (last_seat_ts_.find(state.seat_id) != last_seat_ts_.end() && current_ts_ms > 0) {
        int64_t prev = last_seat_ts_[state.seat_id];
        time_diff_sec = static_cast<int>(std::max<int64_t>(0, (current_ts_ms - prev) / 1000));
    }

    // protection: Avoid excessive accumulation of single frames due to TS jumps
    const int MAX_INC_PER_FRAME = 5; // t(max) that can be accumulated per frame (seconds)
    if (time_diff_sec > MAX_INC_PER_FRAME) time_diff_sec = MAX_INC_PER_FRAME;

    // read detection information
    int person_count = seat_j.value("person_count", 0);
    int object_count = seat_j.value("object_count", 0);
    string occupancy_state = seat_j.value("occupancy_state", string("FREE"));

    // judge current status
    int current_status = 0; // 0=Unseated,1=Seated,2=Occupied

    // case1：person detected
    if (person_count > 0 || occupancy_state == "PERSON") {
        current_status = 1;
        state.confidence = seat_j.value("person_conf", 0.95f);
        anomaly_occupied_duration_[state.seat_id] = 0;  // reset
    }

    // case2：no person but objects detected   
    else if (object_count > 0 || occupancy_state == "OBJECT_ONLY") {

        anomaly_occupied_duration_[state.seat_id] += time_diff_sec;
        current_status = 2;

        // when reach threshold -> trigger alarm
        if (anomaly_occupied_duration_[state.seat_id] >= ANOMALY_THRESHOLD_SECONDS) {
            state.confidence = seat_j.value("object_conf", 0.85f);

            B2CD_Alert alert;
            alert.alert_id = state.seat_id + "_" + a_data.timestamp;
            alert.seat_id = state.seat_id;
            alert.alert_type = "AnomalyOccupied";
            alert.alert_desc = string("Seat occupied by object for ") + to_string(anomaly_occupied_duration_[state.seat_id]) + " seconds";
            alert.timestamp = a_data.timestamp;
            alert.is_processed = false;
            alerts.push_back(alert);
        }
    }

    // case3: no objects/persons
    else {
        anomaly_occupied_duration_[state.seat_id] = 0;
        current_status = 0;
    }

    // update duration
    int prev_status = -1;
    int prev_duration = 0;
    auto it_status = last_seat_status_.find(state.seat_id);
    if (it_status != last_seat_status_.end()) {
        prev_status = it_status->second;
        prev_duration = last_seat_status_duration_[state.seat_id];
    }

    if (prev_status != -1 && prev_status == current_status) {
        state.status_duration = prev_duration + time_diff_sec;
    } else {
        state.status_duration = time_diff_sec;
    }

    bool status_changed = (prev_status == -1) ? true : (prev_status != current_status);

    // persist state
    state.status = static_cast<B2CD_State::SeatStatus>(current_status);
    last_seat_ts_[state.seat_id] = current_ts_ms;
    last_seat_status_[state.seat_id] = current_status;
    last_seat_status_duration_[state.seat_id] = state.status_duration;

    
    B2C_SeatEvent event;
    event.seat_id = state.seat_id;
    event.state = stateToStr(current_status);
    event.timestamp = a_data.timestamp;
    event.duration_sec = state.status_duration;
    out_event = event;

    // snapshot
    out_snapshot.seat_id = state.seat_id;
    out_snapshot.state = stateToStr(current_status);
    out_snapshot.person_count = person_count;
    out_snapshot.timestamp = a_data.timestamp;
}

void SeatStateJudger::run(const string& jsonl_dir) {
    resetNeedStoreFrameIndexes();

    // directory path
    string dir_path = jsonl_dir.empty() ? "../../out" : jsonl_dir;
    fs::path folder(dir_path);

    if (!fs::exists(folder) || !fs::is_directory(folder)) {
        cout << "[B] Error: JSONL directory does not exist: " << dir_path << endl;
        return;
    }

    cout << "[B] Info: B module started monitoring directory: " << fs::absolute(folder).string() << endl;
    while (true) {
        try {
            for (auto& entry : fs::directory_iterator(folder)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".jsonl") continue;

                string file_path = entry.path().string();
                string filename  = entry.path().filename().string();

                if (processed_files_.count(filename)) continue;

                cout << "[B] New file detected: " << filename << endl;

                vector<vector<A2B_Data>> batch_a2b;
                vector<vector<json>> batch_seat_j;

                bool ok = readJsonlFile(file_path, batch_a2b, batch_seat_j);
                if (!ok) {
                    cout << "[B] Failed to read file, skipping: " << filename << endl;
                    processed_files_.insert(filename);
                    continue;
                }

                // 1 line of JSONL = 1 frame
                for (size_t f = 0; f < batch_a2b.size(); ++f) {
                    auto& frame_a2b  = batch_a2b[f];
                    auto& frame_j    = batch_seat_j[f];

                    if (frame_a2b.empty()) continue;

                    int frame_id = frame_a2b[0].frame_id;
                    cout << "[Frame " << frame_id << "] Processing " << frame_a2b.size() << " seats" << endl;

                    bool need_store_this_frame = false;

                    for (size_t i = 0; i < frame_a2b.size(); ++i) {
                        B2CD_State state;
                        vector<B2CD_Alert> alerts;
                        B2C_SeatSnapshot snapshot;
                        optional<B2C_SeatEvent> event;

                        processAData(frame_a2b[i], frame_j[i], state, alerts, snapshot, event);

                        // write to DB
                        if (event.has_value() && db_) {
                            db_->insertSeatEvent(event->seat_id, event->state, event->timestamp, event->duration_sec);
                        }
                        if (db_) {
                            db_->insertSnapshot(snapshot.timestamp, snapshot.seat_id, snapshot.state, snapshot.person_count);
                        }
                        for (auto& a : alerts) {
                            if (db_) db_->insertAlert(a.alert_id, a.seat_id, a.alert_type, a.alert_desc, a.timestamp, a.is_processed);
                        }

                        // cout info
                        cout << "  Seat " << state.seat_id
                             << " : " << stateToStr((int)state.status)
                             << " dur=" << state.status_duration
                             << " conf=" << fixed << setprecision(2)
                             << state.confidence << endl;

                        if (!alerts.empty()) cout << "    Alert: " << alerts[0].alert_desc << endl;

                        if (event.has_value() || !alerts.empty() || state.status != B2CD_State::UNSEATED) {
                            need_store_this_frame = true;
                        }
                    }

                    if (need_store_this_frame) {
                        need_store_frame_indexes_.insert(frame_id);
                        cout << "[B Info] Marked frame " << frame_id << " for storage" << endl;
                    }

                    cout << "-------------------------------------" << endl;
                }

                processed_files_.insert(filename);
            }
        } catch (const std::exception& e) {
            cout << "[B] Error while processing JSONL files: " << e.what() << endl;
        }

        // sleep 2s
        this_thread::sleep_for(chrono::seconds(2));
    }
}
