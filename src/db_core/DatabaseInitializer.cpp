#include "DatabaseInitializer.h"
#include "DatabaseSchemas.h"
#include "TimeUtils.h"
#include <iostream>
#include <vector>

DatabaseInitializer::DatabaseInitializer(SeatDatabase& db) : database_(db) {}

bool DatabaseInitializer::initializeSampleData() {
    try {
        // Clear existing data
        if (!clearExistingData()) {
            return false;
        }

        // Insert sample seat data
        if (!insertSampleSeats()) {
            return false;
        }

        // Insert sample event data
        if (!insertSampleEvents()) {
            return false;
        }

        std::cout << "Sample data initialized successfully." << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Sample data initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseInitializer::clearExistingData() {
    try {
        database_.beginTransaction();

        // Empty tables in the correct order 
        database_.exec("DELETE FROM seat_agg_hourly");
        database_.exec("DELETE FROM seat_snapshots");
        database_.exec("DELETE FROM seat_events");
        database_.exec("DELETE FROM seats");

        database_.commitTransaction();
        return true;

    } catch (const std::exception& e) {
        database_.rollbackTransaction();
        std::cerr << "Failed to clear existing data: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseInitializer::insertSampleSeats() {
    try {
        // Quiet Study Area (quiet)
        database_.insertSeat("S1", 177, 584, 225, 260);
        database_.insertSeat("S2", 428, 525, 225, 228);
        database_.insertSeat("S3", 272, 411, 167, 112);
        database_.insertSeat("S4",  87, 454, 169, 176);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to insert sample seats: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseInitializer::insertSampleEvents() {
    try {
        std::string current_time = TimeUtils::getCurrentTimestamp();
        std::string one_hour_ago = TimeUtils::addHours(current_time, -1);
        std::string two_hours_ago = TimeUtils::addHours(current_time, -2);

        
        for (const auto& seat_id : seat_ids) {
            // 插入当前状态事件（所有座位都是 Unseated）
            database_.insertSeatEvent(seat_id, "Unseated", current_time, 0);
        }
        
        // Insert some historical events
        //database_.insertSeatEvent("S1", "Seated", two_hours_ago, 3600);
        //database_.insertSeatEvent("S2", "Unseated", two_hours_ago, 0);

        // Insert Current State Event
        //database_.insertSeatEvent("S1", "Seated", current_time, 0);
        //database_.insertSeatEvent("S2", "Seated", current_time, 0);
        //database_.insertSeatEvent("S3", "Unseated", current_time, 0);


        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to insert sample events: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseInitializer::exec(const std::string& sql) {
    try {
        database_.exec(sql);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SQL execution failed: " << e.what() << std::endl;
        return false;
    }
}
