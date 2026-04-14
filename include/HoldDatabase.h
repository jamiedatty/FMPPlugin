#pragma once
// ============================================================
//  HoldDatabase.h  —  JSON-backed holding fix database
// ============================================================
#include <string>
#include <vector>
#include <unordered_map>

// One holding fix entry as read from holds.json
struct HoldFix
{
    std::string fix;           // e.g. "TUPAR"
    int         inboundTrack;  // magnetic inbound track (0–359)
    std::string turn;          // "LEFT" or "RIGHT"
    int         altitudeFt;    // minimum hold altitude in feet
    int         legsNm;        // outbound leg length in NM
};

// ============================================================
class CHoldDatabase
{
public:
    CHoldDatabase() = default;

    // Load (or reload) the JSON file at the given path.
    // Returns true on success; on failure the previous data is retained.
    bool Load(const std::string& jsonFilePath);

    // Returns the list of holding fixes defined for the given airport ICAO.
    // Returns an empty vector if the airport is not in the database.
    const std::vector<HoldFix>& GetFixes(const std::string& airportIcao) const;

    // Returns every airport ICAO present in the database.
    std::vector<std::string> GetAirports() const;

    // Returns the specific HoldFix for (airport, fixName), or nullptr.
    const HoldFix* FindFix(const std::string& airportIcao,
                           const std::string& fixName) const;

    bool IsLoaded() const { return m_Loaded; }

private:
    // airport ICAO → list of fixes
    std::unordered_map<std::string, std::vector<HoldFix>> m_Data;
    static const std::vector<HoldFix> s_Empty;
    bool m_Loaded = false;
};
