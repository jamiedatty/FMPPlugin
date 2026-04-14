// ============================================================
//  AdvisoryMessage.cpp  —  Formats pilot advisory messages
// ============================================================
#include "AdvisoryMessage.h"

#include <sstream>
#include <iomanip>

// ============================================================
std::string CAdvisoryMessage::Build(
    const std::string& callsign,
    const std::string& destAirport,
    const HoldFix&     fix) const
{
    std::ostringstream oss;

    oss << "=== TRAFFIC ADVISORY - " << destAirport << " ===\n";
    oss << "TO: " << callsign << "\n";
    oss << "\n";
    oss << "HIGH TRAFFIC DEMAND — EXPECT HOLDING\n";
    oss << "\n";
    oss << "HOLDING FIX  : " << fix.fix << "\n";
    oss << "INBOUND TRACK: " << PadTrack(fix.inboundTrack) << " DEG\n";
    oss << "TURN DIR     : " << fix.turn << " TURNS\n";
    oss << "MIN ALT      : " << fix.altitudeFt << " FT\n";
    oss << "LEG LENGTH   : " << fix.legsNm << " NM\n";
    oss << "\n";
    oss << "NOTE: THIS INFORMATION MAY CHANGE.\n";
    oss << "FOLLOW ATC INSTRUCTIONS AT ALL TIMES.\n";
    oss << "==========================================";

    return oss.str();
}

// ============================================================
std::string CAdvisoryMessage::BuildShort(
    const std::string& callsign,
    const HoldFix&     fix) const
{
    std::ostringstream oss;
    oss << callsign
        << " EXPECT HOLD AT " << fix.fix
        << " TRK " << PadTrack(fix.inboundTrack) << " DEG"
        << " " << fix.turn << " TURNS"
        << " — FOLLOW ATC INSTRUCTIONS";
    return oss.str();
}

// ============================================================
std::string CAdvisoryMessage::PadTrack(int track)
{
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << (track % 360);
    return oss.str();
}
