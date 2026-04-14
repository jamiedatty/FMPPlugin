// ============================================================
//  HoldDatabase.cpp  —  Loads holds.json via nlohmann/json
// ============================================================
#include "HoldDatabase.h"
#include "json.hpp"      // nlohmann/json single-header

#include <fstream>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

const std::vector<HoldFix> CHoldDatabase::s_Empty;

// ── Helper: upper-case a string ───────────────────────────────
static std::string ToUpper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(::toupper(c)); });
    return s;
}

// ============================================================
bool CHoldDatabase::Load(const std::string& jsonFilePath)
{
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) return false;

    try
    {
        json root = json::parse(file);

        std::unordered_map<std::string, std::vector<HoldFix>> newData;

        for (auto& [airport, fixArray] : root.items())
        {
            const std::string icao = ToUpper(airport);
            std::vector<HoldFix> fixes;

            for (const auto& entry : fixArray)
            {
                HoldFix hf;
                hf.fix          = ToUpper(entry.value("fix", ""));
                hf.inboundTrack = entry.value("inbound_track", 0);
                hf.turn         = ToUpper(entry.value("turn", "RIGHT"));
                hf.altitudeFt   = entry.value("alt_ft",   0);
                hf.legsNm       = entry.value("legs_nm",  10);

                if (!hf.fix.empty())
                    fixes.push_back(std::move(hf));
            }

            if (!fixes.empty())
                newData[icao] = std::move(fixes);
        }

        m_Data   = std::move(newData);
        m_Loaded = true;
        return true;
    }
    catch (const json::exception&)
    {
        // Keep existing data on parse failure
        return false;
    }
}

// ============================================================
const std::vector<HoldFix>& CHoldDatabase::GetFixes(
    const std::string& airportIcao) const
{
    auto it = m_Data.find(airportIcao);
    return (it != m_Data.end()) ? it->second : s_Empty;
}

// ============================================================
std::vector<std::string> CHoldDatabase::GetAirports() const
{
    std::vector<std::string> result;
    result.reserve(m_Data.size());
    for (const auto& kv : m_Data)
        result.push_back(kv.first);
    return result;
}

// ============================================================
const HoldFix* CHoldDatabase::FindFix(
    const std::string& airportIcao,
    const std::string& fixName) const
{
    const auto& fixes = GetFixes(airportIcao);
    for (const auto& hf : fixes)
        if (hf.fix == fixName) return &hf;
    return nullptr;
}
