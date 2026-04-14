#pragma once
// ============================================================
//  AdvisoryMessage.h  —  Builds and formats pilot advisory text
// ============================================================
#include "HoldDatabase.h"
#include <string>

class CAdvisoryMessage
{
public:
    CAdvisoryMessage() = default;

    // Build the full advisory message string to send to the pilot.
    // Returns a formatted multi-line string ready for SendTextMessage().
    std::string Build(
        const std::string& callsign,
        const std::string& destAirport,
        const HoldFix&     fix) const;

    // Short single-line version for the ES chat (no line breaks).
    std::string BuildShort(
        const std::string& callsign,
        const HoldFix&     fix) const;

    // Zero-pads a magnetic track to 3 digits: 32 → "032"
    static std::string PadTrack(int track);
};
