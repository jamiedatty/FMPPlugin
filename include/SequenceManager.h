#pragma once
// ============================================================
//  SequenceManager.h  —  Arrival sequence numbering and
//                         delay absorption calculation
//
//  Assigns landing sequence positions to inbound aircraft
//  based on ETA, and estimates how many minutes of holding
//  each aircraft needs to absorb before landing.
// ============================================================
#include "FlowManager.h"

#include <string>
#include <vector>
#include <ctime>

// Result of sequencing one aircraft
struct SequenceResult
{
    std::string callsign;
    int         position;       // 1-based position in the sequence
    std::time_t scheduledLand;  // UTC epoch seconds of assigned landing slot
    int         delayMinutes;   // extra delay vs original ETA (0 = no delay)
    std::string tag;            // formatted tag string e.g. "S03 +12"
};

// ============================================================
class CSequenceManager
{
public:
    // separationMinutes: minimum time between successive landings
    explicit CSequenceManager(int separationMinutes = 5);

    // Sequence all aircraft for the given airport.
    // Input is already sorted by ETA (FlowManager provides this).
    // Returns one SequenceResult per aircraft, in sequence order.
    std::vector<SequenceResult> Sequence(
        const std::vector<AircraftFlowData>& arrivals) const;

    // Format the sequence tag for the radar label (max 7 chars)
    // e.g. "S03+12" — position 3, +12 min delay
    //      "S01   " — position 1, no delay
    static std::string FormatTag(int pos, int delayMin);

    void SetSeparationMinutes(int n) { m_SepMin = n; }
    int  GetSeparationMinutes() const { return m_SepMin; }

private:
    int m_SepMin;
};
