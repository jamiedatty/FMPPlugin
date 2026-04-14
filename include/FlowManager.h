#pragma once
// ============================================================
//  FlowManager.h  —  Per-airport arrival flow tracking
//
//  Tracks:
//    - How many aircraft are currently holding per airport
//    - Estimated landing times (ELT) derived from flight plan ETA
//    - Whether demand exceeds a configured threshold
// ============================================================
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <ctime>

// Snapshot of one aircraft's flow data
struct AircraftFlowData
{
    std::string callsign;
    std::string destination;
    std::string assignedFix;      // empty if not holding
    bool        isHolding = false;

    // ETA as UTC epoch seconds (0 = unknown)
    std::time_t etaUtc = 0;

    // Sequence position within the airport arrival stream (1-based, 0 = unsequenced)
    int sequencePos = 0;

    // Estimated delay in minutes due to holding
    int delayMinutes = 0;
};

// Per-airport demand summary
struct AirportDemand
{
    std::string airport;
    int         totalInbound   = 0;   // all aircraft with this destination
    int         holdingCount   = 0;   // aircraft with an assigned fix
    int         demandLevel    = 0;   // 0=Low 1=Medium 2=High (vs threshold)
};

// ============================================================
class CFlowManager
{
public:
    explicit CFlowManager(int highDemandThreshold = 3);

    // Called by the plugin whenever aircraft state changes
    void UpdateAircraft(const AircraftFlowData& data);
    void RemoveAircraft(const std::string& callsign);

    // Returns current demand summary for an airport
    AirportDemand GetDemand(const std::string& airport) const;

    // Returns all airports with at least one tracked aircraft
    std::vector<std::string> GetActiveAirports() const;

    // Returns all aircraft for a given airport, sorted by ETA
    std::vector<AircraftFlowData> GetAircraftForAirport(
        const std::string& airport) const;

    // Returns the demand level label string
    static std::string DemandLabel(int level);

    // Update the threshold
    void SetHighDemandThreshold(int n) { m_Threshold = n; }
    int  GetHighDemandThreshold() const { return m_Threshold; }

private:
    void RebuildDemand(const std::string& airport);

    // callsign → data
    std::unordered_map<std::string, AircraftFlowData> m_Aircraft;

    // airport → demand
    std::unordered_map<std::string, AirportDemand> m_Demand;

    int m_Threshold = 3;
};
