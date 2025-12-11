#include "SeatDatabase.h"
#include "DatabaseSchemas.h"
#include "DataTypes.h"
//#include "../utils/TimeUtils.h"
#include "TimeUtils.h"
#include <iostream>
#include <sstream>

SeatDatabase::SeatDatabase(const std::string& db_path) : db_path_(db_path) {
    try {
        database_ = std::make_unique<SQLite::Database>(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "Database opened successfully: " << db_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to open database: " << e.what() << std::endl;
        throw;
    }
}

// Singleton Pattern Implementation
SeatDatabase& SeatDatabase::getInstance(const std::string& db_path) {
    static SeatDatabase instance(db_path);
    return instance;
}

bool SeatDatabase::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        bool success = createTables();
        if (success) {
            std::cout << "Database initialized successfully." << std::endl;
        }
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Database initialization failed: " << e.what() << std::endl;
        return false;
    }
}

// Create Table
bool SeatDatabase::createTables() {
    try {
        database_->exec(DatabaseSchemas::CREATE_SEATS_TABLE);
        database_->exec(DatabaseSchemas::CREATE_SEAT_EVENTS_TABLE);
        database_->exec(DatabaseSchemas::CREATE_SEAT_SNAPSHOTS_TABLE);
        database_->exec(DatabaseSchemas::CREATE_SEAT_AGG_HOURLY_TABLE);
        database_->exec(DatabaseSchemas::CREATE_ALERTS_TABLE);
        std::cout << "All tables created successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Table creation failed: " << e.what() << std::endl;
        return false;
    }
}

// Insert Seat Event - Align with B2C_SeatEvent of Module B
bool SeatDatabase::insertSeatEvent(const std::string& seat_id, 
                                  const std::string& state, 
                                  const std::string& timestamp, 
                                  int duration_sec) {

    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        SQLite::Statement query(*database_, 
            "INSERT INTO seat_events (seat_id, state, timestamp, duration_sec) VALUES (?, ?, ?, ?)");
        
        query.bind(1, seat_id);
        query.bind(2, state);
        query.bind(3, timestamp);
        query.bind(4, duration_sec);
        
        bool success = query.exec() == 1;
        if (success) {
            std::cout << "Seat insertion event successful: " << seat_id << " " << state << std::endl;
        }
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Insert seat event failed: " << e.what() << std::endl;
        return false;
    }
}

// Insert snapshot data - align with B module's B2C_SeatSnapshot
bool SeatDatabase::insertSnapshot(const std::string& timestamp, 
                                 const std::string& seat_id, 
                                 const std::string& state, 
                                 int person_count) {

    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        SQLite::Statement query(*database_, 
            "INSERT INTO seat_snapshots (timestamp, seat_id, state, person_count) VALUES (?, ?, ?, ?)");
        
        query.bind(1, timestamp);
        query.bind(2, seat_id);
        query.bind(3, state);
        query.bind(4, person_count);
        
        return query.exec() == 1;
    } catch (const std::exception& e) {
        std::cerr << "Insert snapshot failed: " << e.what() << std::endl;
        return false;
    }
}

// Insert hourly aggregated data
bool SeatDatabase::insertHourlyAggregation(const std::string& date_hour, 
                                          const std::string& seat_id, 
                                          int occupied_minutes) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        SQLite::Statement query(*database_, 
            "INSERT OR REPLACE INTO seat_agg_hourly (date_hour, seat_id, occupied_minutes) VALUES (?, ?, ?)");
        
        query.bind(1, date_hour);
        query.bind(2, seat_id);
        query.bind(3, occupied_minutes);
        
        return query.exec() == 1;
    } catch (const std::exception& e) {
        std::cerr << "Insert hourly aggregation failed: " << e.what() << std::endl;
        return false;
    }
}

bool SeatDatabase::insertSeat(const std::string& seat_id, 
                             int roi_x, int roi_y, 
                             int roi_width, int roi_height) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        SQLite::Statement query(*database_, 
            "INSERT OR REPLACE INTO seats (seat_id, roi_x, roi_y, roi_width, roi_height) VALUES (?, ?, ?, ?, ?)");
        
        query.bind(1, seat_id);
        query.bind(2, roi_x);
        query.bind(3, roi_y);
        query.bind(4, roi_width);
        query.bind(5, roi_height);
        
        return query.exec() == 1;
    } catch (const std::exception& e) {
        std::cerr << "Insert seat failed: " << e.what() << std::endl;
        return false;
    }
}

// Insert alarm data
bool SeatDatabase::insertAlert(
    const std::string& alert_id,
    const std::string& seat_id,
    const std::string& alert_type,
    const std::string& alert_desc,
    const std::string& timestamp,
    bool is_processed) {
    
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        SQLite::Statement query(*database_,
            "INSERT INTO alerts (alert_id, seat_id, alert_type, alert_desc, timestamp, is_processed) VALUES (?, ?, ?, ?, ?, ?)");
        
        query.bind(1, alert_id);
        query.bind(2, seat_id);
        query.bind(3, alert_type);
        query.bind(4, alert_desc);
        query.bind(5, timestamp);
        query.bind(6, is_processed ? 1 : 0);
        
        bool success = query.exec() == 1;
        if (success) {
            std::cout << "Alert inserted: " << alert_id << " for seat " << seat_id << std::endl;
        }
        return success;
    } catch (const std::exception& e) {
        std::cerr << "Insert alert failed: " << e.what() << std::endl;
        return false;
    }
}

// Get Unprocessed Alerts
std::vector<AlertData> SeatDatabase::getUnprocessedAlerts() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<AlertData> alerts;
    
    try {
        SQLite::Statement query(*database_,
            "SELECT alert_id, seat_id, alert_type, alert_desc, timestamp, is_processed FROM alerts WHERE is_processed = 0 ORDER BY timestamp DESC");
        
        while (query.executeStep()) {
            AlertData alert;
            alert.alert_id = query.getColumn(0).getString();
            alert.seat_id = query.getColumn(1).getString();
            alert.alert_type = query.getColumn(2).getString();
            alert.alert_desc = query.getColumn(3).getString();
            alert.timestamp = query.getColumn(4).getString();
            alert.is_processed = query.getColumn(5).getInt() != 0;
            alerts.push_back(alert);
        }
    } catch (const std::exception& e) {
        std::cerr << "Get unprocessed alerts failed: " << e.what() << std::endl;
    }
    
    return alerts;
}

// Mark alert as resolved
bool SeatDatabase::markAlertAsProcessed(const std::string& alert_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        SQLite::Statement query(*database_,
            "UPDATE alerts SET is_processed = 1 WHERE alert_id = ?");
        
        query.bind(1, alert_id);
        return query.exec() == 1;
    } catch (const std::exception& e) {
        std::cerr << "Mark alert as processed failed: " << e.what() << std::endl;
        return false;
    }
}

// Get current seat status
std::vector<SeatStatus> SeatDatabase::getCurrentSeatStatus() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<SeatStatus> results;
    
    try {
        SQLite::Statement query(*database_, R"(
            SELECT s.seat_id, e.state, MAX(e.timestamp) as last_update
            FROM seats s
            LEFT JOIN seat_events e ON s.seat_id = e.seat_id
            WHERE e.timestamp = (SELECT MAX(timestamp) FROM seat_events WHERE seat_id = s.seat_id)
            GROUP BY s.seat_id
        )");
        
        while (query.executeStep()) {
            SeatStatus status;
            status.seat_id = query.getColumn(0).getString();
            status.state = query.getColumn(1).getString();
            status.last_update = query.getColumn(2).getString();
            results.push_back(status);
        }
    } catch (const std::exception& e) {
        std::cerr << "Get current seat status failed: " << e.what() << std::endl;
    }
    
    return results;
}

// Get basic statistics
BasicStats SeatDatabase::getCurrentBasicStats() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    BasicStats stats;
    stats.total_seats = 0;
    stats.occupied_seats = 0;
    stats.anomaly_seats = 0;
    stats.overall_occupancy_rate = 0.0;
    
    try {
        // Get total number of seats
        SQLite::Statement totalQuery(*database_, "SELECT COUNT(*) FROM seats");
        if (totalQuery.executeStep()) {
            stats.total_seats = totalQuery.getColumn(0).getInt();
        }
        
        // Get the number of currently occupied and abnormal seats
        SQLite::Statement statusQuery(*database_, R"(
            SELECT state, COUNT(*) 
            FROM (
                SELECT s.seat_id, e.state
                FROM seats s
                LEFT JOIN seat_events e ON s.seat_id = e.seat_id
                WHERE e.timestamp = (SELECT MAX(timestamp) FROM seat_events WHERE seat_id = s.seat_id)
            ) GROUP BY state
        )");
        
        while (statusQuery.executeStep()) {
            std::string state = statusQuery.getColumn(0).getString();
            int count = statusQuery.getColumn(1).getInt();
            
            if (state == "Seated") {
                stats.occupied_seats = count;
            } else if (state == "Anomaly") {
                stats.anomaly_seats = count;
            }
        }
        
        // Calculate overall occupancy rate
        if (stats.total_seats > 0) {
            stats.overall_occupancy_rate = static_cast<double>(stats.occupied_seats + stats.anomaly_seats) / stats.total_seats;
        }
    } catch (const std::exception& e) {
        std::cerr << "Get basic stats failed: " << e.what() << std::endl;
    }
    
    return stats;
}

// Get occupied minutes
int SeatDatabase::getOccupiedMinutes(const std::string& seat_id, 
                                    const std::string& start_time, 
                                    const std::string& end_time) {

    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        SQLite::Statement query(*database_, R"(
            SELECT SUM(duration_sec) 
            FROM seat_events 
            WHERE seat_id = ? 
            AND timestamp BETWEEN ? AND ?
            AND state IN ('Seated', 'Anomaly')
        )");
        
        query.bind(1, seat_id);
        query.bind(2, start_time);
        query.bind(3, end_time);
        
        if (query.executeStep()) {
            int total_seconds = query.getColumn(0).getInt();
            return total_seconds / 60; // Convert to minutes
        }
    } catch (const std::exception& e) {
        std::cerr << "Get occupied minutes failed: " << e.what() << std::endl;
    }

    return 0;
}

// Get today's hourly data
std::vector<HourlyData> SeatDatabase::getTodayHourlyData() {
    std::vector<HourlyData> result;
    
    try {
        // Get the current date
        std::string today = TimeUtils::formatTimestamp(getCurrentTimestamp(), "%Y-%m-%d");
        // Query hourly data
        auto hourly_rates = getDailyHourlyOccupancy(today);
        
        for (int hour = 0; hour < 24; ++hour) {
            std::string hour_str = (hour < 10 ? "0" : "") + std::to_string(hour) + ":00";
            result.emplace_back(hour_str, hourly_rates[hour]);
        }
    } catch (const std::exception& e) {
        std::cerr << "Get today hourly data failed: " << e.what() << std::endl;
    }
    
    return result;
}

// Overall Utilization Calculation
double SeatDatabase::getOverallOccupancyRate(const std::string& date_hour) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    
    try {
        // Get the total number of seats for that hour
        SQLite::Statement totalQuery(*database_, "SELECT COUNT(*) FROM seats");
        int total_seats = 0;
        if (totalQuery.executeStep()) {
            total_seats = totalQuery.getColumn(0).getInt();
        }
        
        if (total_seats == 0) return 0.0;
        
        // Get the number of occupied seats for that hour
        SQLite::Statement occupiedQuery(*database_, R"(
            SELECT COUNT(DISTINCT seat_id) 
            FROM seat_events 
            WHERE timestamp BETWEEN ? AND datetime(?, '+1 hour')
            AND state IN ('Seated', 'Anomaly')
        )");
        
        occupiedQuery.bind(1, date_hour);
        occupiedQuery.bind(2, date_hour);
        
        int occupied_seats = 0;
        if (occupiedQuery.executeStep()) {
            occupied_seats = occupiedQuery.getColumn(0).getInt();
        }
        
        return static_cast<double>(occupied_seats) / total_seats;
        
    } catch (const std::exception& e) {
        std::cerr << "Get overall occupancy rate failed: " << e.what() << std::endl;
        return 0.0;
    }
}

// Daily Hour Utilization Rate
std::vector<double> SeatDatabase::getDailyHourlyOccupancy(const std::string& date) {
    std::vector<double> hourly_rates(24, 0.0);
    
    try {
        for (int hour = 0; hour < 24; ++hour) {
            std::string date_hour = date + " " + 
                                  (hour < 10 ? "0" : "") + 
                                  std::to_string(hour) + ":00:00";
            hourly_rates[hour] = getOverallOccupancyRate(date_hour);
        }
    } catch (const std::exception& e) {
        std::cerr << "Get daily hourly occupancy failed: " << e.what() << std::endl;
    }
    
    return hourly_rates;
}


// Start transaction
bool SeatDatabase::beginTransaction() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        database_->exec("BEGIN TRANSACTION");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Begin transaction failed: " << e.what() << std::endl;
        return false;
    }
}

// Commit transaction
bool SeatDatabase::commitTransaction() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        database_->exec("COMMIT");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Commit transaction failed: " << e.what() << std::endl;
        return false;
    }
}

// Rollback transaction
bool SeatDatabase::rollbackTransaction() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        database_->exec("ROLLBACK");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Rollback transaction failed: " << e.what() << std::endl;
        return false;
    }
}

// Utility method
std::vector<std::string> SeatDatabase::getAllSeatIds() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<std::string> seat_ids;
    
    try {
        SQLite::Statement query(*database_, "SELECT seat_id FROM seats");
        
        while (query.executeStep()) {
            seat_ids.push_back(query.getColumn(0).getString());
        }
    } catch (const std::exception& e) {
        std::cerr << "Get all seat ids failed: " << e.what() << std::endl;
    }
    
    return seat_ids;
}

std::string SeatDatabase::getCurrentTimestamp() {
    // Simple implementation, can also use TimeUtils
    time_t now = time(nullptr);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buffer);
}


bool SeatDatabase::exec(const std::string& sql) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    try {
        database_->exec(sql);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Execute SQL failed: " << e.what() << std::endl;
        std::cerr << "SQL: " << sql << std::endl;
        return false;
    }
}
