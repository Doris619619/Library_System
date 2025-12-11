#ifndef DATABASE_INITIALIZER_H
#define DATABASE_INITIALIZER_H

#include "SeatDatabase.h"
//#include "../utils/TimeUtils.h"
#include "TimeUtils.h"
#include <string>

class DatabaseInitializer {
public:
    DatabaseInitializer(SeatDatabase& db);
    
    // Initialize sample data
    bool initializeSampleData();
    
    // Clear existing data
    bool clearExistingData();
    
    // Insert sample seat
    bool insertSampleSeats();
    
    // Insert sample event
    bool insertSampleEvents();
    
private:
    SeatDatabase& database_;
    
    // Execute SQL statement
    bool exec(const std::string& sql);
};

#endif // DATABASE_INITIALIZER_H
