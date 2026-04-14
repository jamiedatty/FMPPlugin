// ============================================================
//  SequenceManager.cpp
// ============================================================
#include "SequenceManager.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

// ============================================================
CSequenceManager::CSequenceManager(int separationMinutes)
    : m_SepMin(separationMinutes)
{}

// ============================================================
std::vector<SequenceResult> CSequenceManager::Sequence(
    const std::vector<AircraftFlowData>& arrivals) const
{
    std::vector<SequenceResult> results;
    results.reserve(arrivals.size());

    // Work through arrivals in ETA order.
    // Each aircraft gets a landing slot no earlier than:
    //   max(its own ETA, previous slot + separation)
    std::time_t nextSlot = 0;

    for (int i = 0; i < static_cast<int>(arrivals.size()); ++i)
    {
        const AircraftFlowData& ac = arrivals[i];
        SequenceResult res;
        res.callsign = ac.callsign;
        res.position = i + 1;

        if (ac.etaUtc == 0)
        {
            // No ETA — cannot compute delay
            res.scheduledLand = 0;
            res.delayMinutes  = 0;
            res.tag           = FormatTag(res.position, 0);
        }
        else
        {
            // First slot starts at the first aircraft's ETA
            if (nextSlot == 0) nextSlot = ac.etaUtc;

            std::time_t slot = std::max(ac.etaUtc, nextSlot);
            int delay = static_cast<int>(
                std::round((slot - ac.etaUtc) / 60.0));

            res.scheduledLand = slot;
            res.delayMinutes  = std::max(0, delay);
            res.tag           = FormatTag(res.position, res.delayMinutes);

            // Next aircraft cannot land before this slot + separation
            nextSlot = slot + (m_SepMin * 60);
        }

        results.push_back(res);
    }

    return results;
}

// ============================================================
std::string CSequenceManager::FormatTag(int pos, int delayMin)
{
    // Format: "S##" or "S##+##" — max 7 characters for the tag column
    std::ostringstream oss;
    oss << "S" << std::setw(2) << std::setfill('0') << pos;
    if (delayMin > 0)
        oss << "+" << std::setw(2) << std::setfill('0') << delayMin;
    return oss.str();
}
