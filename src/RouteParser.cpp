// ============================================================
//  RouteParser.cpp  —  Tokenises flight plan routes and matches
//                       against the hold database
// ============================================================
#include "RouteParser.h"

#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_set>

// ============================================================
std::vector<std::string> CRouteParser::FindHoldFixes(
    const std::string&   routeString,
    const std::string&   destAirport,
    const CHoldDatabase& db) const
{
    // Build a fast lookup set of known fixes for this airport
    const auto& knownFixes = db.GetFixes(destAirport);
    if (knownFixes.empty()) return {};

    std::unordered_set<std::string> fixSet;
    fixSet.reserve(knownFixes.size());
    for (const auto& hf : knownFixes)
        fixSet.insert(hf.fix);

    // Tokenise and scan in route order — preserve first-occurrence order
    auto tokens = Tokenise(routeString);

    std::vector<std::string> result;
    std::unordered_set<std::string> seen;

    for (const auto& raw : tokens)
    {
        std::string tok = Normalise(raw);

        // A route token can be  WAYPOINT/FL340  (speed-level group)
        // Strip everything from '/' onwards
        auto slashPos = tok.find('/');
        if (slashPos != std::string::npos)
            tok = tok.substr(0, slashPos);

        if (tok.empty()) continue;
        if (seen.count(tok)) continue;

        if (fixSet.count(tok))
        {
            result.push_back(tok);
            seen.insert(tok);
        }
    }

    return result;
}

// ============================================================
std::vector<std::string> CRouteParser::Tokenise(const std::string& route)
{
    std::istringstream ss(route);
    std::vector<std::string> tokens;
    std::string tok;
    while (ss >> tok)
        tokens.push_back(tok);
    return tokens;
}

// ============================================================
std::string CRouteParser::Normalise(const std::string& token)
{
    std::string result;
    result.reserve(token.size());
    for (unsigned char c : token)
    {
        if (std::isspace(c)) continue;
        result += static_cast<char>(std::toupper(c));
    }
    return result;
}
