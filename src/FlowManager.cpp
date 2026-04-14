// ============================================================
//  FlowManager.cpp
// ============================================================
#include "FlowManager.h"

#include <algorithm>

// ============================================================
CFlowManager::CFlowManager(int highDemandThreshold)
    : m_Threshold(highDemandThreshold)
{}

// ============================================================
void CFlowManager::UpdateAircraft(const AircraftFlowData& data)
{
    const std::string oldDest = [&]() -> std::string {
        auto it = m_Aircraft.find(data.callsign);
        return it != m_Aircraft.end() ? it->second.destination : "";
    }();

    m_Aircraft[data.callsign] = data;

    // If airport changed, rebuild old airport too
    if (!oldDest.empty() && oldDest != data.destination)
        RebuildDemand(oldDest);

    RebuildDemand(data.destination);
}

// ============================================================
void CFlowManager::RemoveAircraft(const std::string& callsign)
{
    auto it = m_Aircraft.find(callsign);
    if (it == m_Aircraft.end()) return;

    const std::string dest = it->second.destination;
    m_Aircraft.erase(it);
    RebuildDemand(dest);
}

// ============================================================
AirportDemand CFlowManager::GetDemand(const std::string& airport) const
{
    auto it = m_Demand.find(airport);
    if (it == m_Demand.end())
    {
        AirportDemand empty;
        empty.airport = airport;
        return empty;
    }
    return it->second;
}

// ============================================================
std::vector<std::string> CFlowManager::GetActiveAirports() const
{
    std::vector<std::string> result;
    for (const auto& kv : m_Demand)
        if (kv.second.totalInbound > 0)
            result.push_back(kv.first);
    return result;
}

// ============================================================
std::vector<AircraftFlowData> CFlowManager::GetAircraftForAirport(
    const std::string& airport) const
{
    std::vector<AircraftFlowData> result;
    for (const auto& kv : m_Aircraft)
        if (kv.second.destination == airport)
            result.push_back(kv.second);

    // Sort by ETA (unknown ETAs go last)
    std::sort(result.begin(), result.end(),
        [](const AircraftFlowData& a, const AircraftFlowData& b)
        {
            if (a.etaUtc == 0 && b.etaUtc == 0) return false;
            if (a.etaUtc == 0) return false;
            if (b.etaUtc == 0) return true;
            return a.etaUtc < b.etaUtc;
        });

    return result;
}

// ============================================================
std::string CFlowManager::DemandLabel(int level)
{
    switch (level)
    {
        case 0:  return "LOW";
        case 1:  return "MED";
        default: return "HIGH";
    }
}

// ============================================================
void CFlowManager::RebuildDemand(const std::string& airport)
{
    AirportDemand d;
    d.airport = airport;

    for (const auto& kv : m_Aircraft)
    {
        if (kv.second.destination != airport) continue;
        ++d.totalInbound;
        if (kv.second.isHolding) ++d.holdingCount;
    }

    if (d.totalInbound == 0)
        d.demandLevel = 0;
    else if (d.holdingCount < m_Threshold / 2)
        d.demandLevel = 0;
    else if (d.holdingCount < m_Threshold)
        d.demandLevel = 1;
    else
        d.demandLevel = 2;

    m_Demand[airport] = d;
}
