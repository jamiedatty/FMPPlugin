#pragma once
// ============================================================
//  RouteParser.h  —  Extracts holding fix candidates from a
//                    filed flight plan route string
// ============================================================
#include "HoldDatabase.h"

#include <string>
#include <vector>

class CRouteParser
{
public:
    CRouteParser() = default;

    // Parse `routeString` (e.g. "UPGAS UL607 DOLIR L607 TUPAR")
    // and return the names of any fixes from `db` that appear in
    // the route for the given destination airport.
    //
    // The returned list preserves the order the fixes appear in the route.
    std::vector<std::string> FindHoldFixes(
        const std::string&  routeString,
        const std::string&  destAirport,
        const CHoldDatabase& db) const;

private:
    // Split a route string into individual tokens (waypoints / airways)
    static std::vector<std::string> Tokenise(const std::string& route);

    // Normalise a token: strip leading/trailing whitespace, convert to upper
    static std::string Normalise(const std::string& token);
};
