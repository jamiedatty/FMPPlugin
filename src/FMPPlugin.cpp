#include "FMPPlugin.h"

#include <Windows.h>
#include <shlwapi.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>

// ── DLL module handle — captured in DllMain ───────────────────
static HMODULE g_hDllModule = nullptr;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        g_hDllModule = hinstDLL;
    return TRUE;
}

// -- EuroScope tag colour codes -------------------------------
static constexpr int TAG_COLOR_DEFAULT = EuroScopePlugIn::TAG_COLOR_DEFAULT;
static constexpr int TAG_COLOR_YELLOW  = EuroScopePlugIn::TAG_COLOR_INFORMATION;
// TAG_COLOR_ONGOING may not exist in older EuroScope SDKs.
// If you get a compile error here, replace with TAG_COLOR_DEFAULT or
// check your EuroScopePlugIn.h for the correct green tag colour constant.
static constexpr int TAG_COLOR_GREEN   = EuroScopePlugIn::TAG_COLOR_ASSUMED;

// -- GUI colour palette ---------------------------------------
static constexpr COLORREF CLR_BG_PANEL   = RGB( 30,  30,  35);
static constexpr COLORREF CLR_BG_TITLE   = RGB( 20,  80, 160);
static constexpr COLORREF CLR_BG_TAB_ON  = RGB( 50,  50,  60);
static constexpr COLORREF CLR_BG_TAB_OFF = RGB( 22,  22,  28);
static constexpr COLORREF CLR_BG_BTN     = RGB( 40, 100, 190);
static constexpr COLORREF CLR_BG_BTN_HOT = RGB( 60, 140, 230);
static constexpr COLORREF CLR_BG_TOGGLE  = RGB( 20,  80, 160);
static constexpr COLORREF CLR_TEXT_WHITE = RGB(230, 230, 230);
static constexpr COLORREF CLR_TEXT_GREY  = RGB(150, 150, 155);
static constexpr COLORREF CLR_TEXT_AMBER = RGB(255, 180,  40);
static constexpr COLORREF CLR_TEXT_GREEN = RGB( 80, 220, 100);
static constexpr COLORREF CLR_TEXT_RED   = RGB(220,  60,  60);
static constexpr COLORREF CLR_BORDER     = RGB( 80,  80,  90);

// -- Helpers --------------------------------------------------
namespace {
    std::string GetDatabasePath()
    {
        char dllPath[MAX_PATH] = {};
        // Use the HINSTANCE captured in DllMain — always points to this DLL,
        // even when loaded via a plugin host like EuroScope.
        GetModuleFileName(g_hDllModule, dllPath, MAX_PATH);
        char* lastSlash = strrchr(dllPath, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
        return std::string(dllPath) + "holds.json";
    }

    // Fill a GDI rectangle with a solid colour
    void FillRect(HDC hDC, RECT r, COLORREF c)
    {
        HBRUSH br = CreateSolidBrush(c);
        ::FillRect(hDC, &r, br);
        DeleteObject(br);
    }

    // Draw a 1-pixel border around r
    void FrameRect(HDC hDC, RECT r, COLORREF c)
    {
        HPEN pen  = CreatePen(PS_SOLID, 1, c);
        HPEN old  = (HPEN)SelectObject(hDC, pen);
        MoveToEx(hDC, r.left,      r.top,    nullptr);
        LineTo  (hDC, r.right - 1, r.top);
        LineTo  (hDC, r.right - 1, r.bottom - 1);
        LineTo  (hDC, r.left,      r.bottom - 1);
        LineTo  (hDC, r.left,      r.top);
        SelectObject(hDC, old);
        DeleteObject(pen);
    }

    // Draw white text left-aligned inside a rect
    void DrawText(HDC hDC, RECT r, const char* txt,
                  COLORREF col = CLR_TEXT_WHITE, UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE)
    {
        SetTextColor(hDC, col);
        SetBkMode(hDC, TRANSPARENT);
        ::DrawTextA(hDC, txt, -1, &r, fmt);
    }

    COLORREF DemandColor(int level)
    {
        if (level >= 2) return CLR_TEXT_RED;
        if (level == 1) return CLR_TEXT_AMBER;
        return CLR_TEXT_GREEN;
    }
} // namespace

// ============================================================
//  CFMPPlugin - Construction / destruction
// ============================================================
CFMPPlugin::CFMPPlugin()
    : CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
              PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR, PLUGIN_COPYRIGHT)
{
    RegisterTagItemType("Hold Advisor",    TAG_ITEM_HOLD_STATUS);
LogMessage("FMP v1.1.2 DEBUG: Registered Hold Details ID=" + std::to_string(TAG_ITEM_HOLD_DETAILS));
RegisterTagItemType("Hold Details v1.1.2",     TAG_ITEM_HOLD_DETAILS);
    RegisterTagItemType("Sequence Status", TAG_ITEM_SEQ_STATUS);
    RegisterTagItemFunction("Assign Hold Fix", TAG_FUNC_HOLD_MENU);
    RegisterTagItemFunction("Sequence Info",   TAG_FUNC_SEQ_MENU);
    RegisterTagItemFunction("Airport Demand",  TAG_FUNC_DEMAND_INFO);

    ReloadDatabase();

    LogMessage("FMP Hold Advisor v" + std::string(PLUGIN_VERSION) + " loaded.  "
               "Click the [FMP] button on any radar screen to open the panel.");
    if (!m_HoldDB.IsLoaded())
        LogMessage("WARNING: holds.json not found or invalid - check plugin folder.");
}

CFMPPlugin::~CFMPPlugin() = default;

// ============================================================
//  OnRadarScreenCreated - return a new CFMPScreen
// ============================================================
EuroScopePlugIn::CRadarScreen*
CFMPPlugin::OnRadarScreenCreated(const char* /*sDisplayName*/,
                                  bool /*NeedRadarContent*/,
                                  bool /*GeoReferenced*/,
                                  bool /*CanBeSaved*/,
                                  bool /*CanBeCreated*/)
{
    CFMPScreen* screen = new CFMPScreen(this);
    m_Screens.push_back(screen);   // non-owning; ES destroys it
    return screen;
}

// ============================================================
//  OnGetTagItem
// ============================================================
void CFMPPlugin::OnGetTagItem(
    EuroScopePlugIn::CFlightPlan  fp,
    EuroScopePlugIn::CRadarTarget /*rt*/,
    int    itemCode,
    int    /*tagData*/,
    char  *sItemString,
    int   *pColorCode,
    COLORREF * /*pRGB*/,
    double * /*pFontSize*/)
{
    if (itemCode == TAG_ITEM_HOLD_STATUS)
    {
        if (!fp.IsValid()) return;
        const std::string callsign = fp.GetCallsign();
        auto it = m_States.find(callsign);
        if (it == m_States.end())
        {
        strncpy_s(sItemString, 16, "HLD", _TRUNCATE);
            *pColorCode = TAG_COLOR_DEFAULT;
            return;
        }
        const AircraftHoldState& state = it->second;
        strncpy_s(sItemString, 16, BuildTagString(state).c_str(), _TRUNCATE);
        if (!state.assignedFix.empty())      *pColorCode = TAG_COLOR_GREEN;
        else if (!state.candidateFixes.empty()) *pColorCode = TAG_COLOR_YELLOW;
        else                                 *pColorCode = TAG_COLOR_DEFAULT;
        return;
    }

    if (itemCode == TAG_ITEM_HOLD_DETAILS)
    {
        if (!fp.IsValid()) {
            strncpy_s(sItemString, 16, "", _TRUNCATE);
            return;
        }
        const std::string callsign = fp.GetCallsign();
        const std::string dest = fp.GetFlightPlanData().GetDestination();
        auto it = m_States.find(callsign);
        if (it == m_States.end() || it->second.assignedFix.empty()) {
            strncpy_s(sItemString, 16, "", _TRUNCATE);
            return;
        }
        const AircraftHoldState& state = it->second;
        const HoldFix* hf = m_HoldDB.FindFix(dest, state.assignedFix);
        if (!hf) {
            strncpy_s(sItemString, 16, "FIX?", _TRUNCATE);
            *pColorCode = TAG_COLOR_YELLOW;
            return;
        }
        // Format: FIX T TRK legs (e.g. TUPAR R 250 10)
        std::ostringstream oss;
        oss << state.assignedFix.substr(0,5) 
            << " " << hf->turn[0]
            << hf->inboundTrack / 10
            << hf->legsNm;
        strncpy_s(sItemString, 16, oss.str().c_str(), _TRUNCATE);
        *pColorCode = TAG_COLOR_GREEN;
        return;
    }

    if (itemCode == TAG_ITEM_SEQ_STATUS)
    {
        if (!fp.IsValid()) return;
        const std::string callsign = fp.GetCallsign();
        auto it = m_States.find(callsign);
        if (it == m_States.end() || it->second.sequencePos == 0)
        {
            strncpy_s(sItemString, 16, "---", _TRUNCATE);
            *pColorCode = TAG_COLOR_DEFAULT;
            return;
        }
        strncpy_s(sItemString, 16, it->second.sequenceTag.c_str(), _TRUNCATE);
        *pColorCode = (it->second.delayMinutes > 0) ? TAG_COLOR_YELLOW : TAG_COLOR_GREEN;
        return;
    }
}

// ============================================================
//  OnFunctionCall
// ============================================================
void CFMPPlugin::OnFunctionCall(int functionId, const char* sItemString, POINT pt, RECT area)
{
    (void)pt;

    if (functionId == TAG_FUNC_SEQ_MENU)
    {
        EuroScopePlugIn::CFlightPlan fp = FlightPlanSelectASEL();
        if (!fp.IsValid()) return;

        const std::string callsign = fp.GetCallsign();
        auto it = m_States.find(callsign);
        if (it == m_States.end() || it->second.sequencePos == 0)
        {
            LogMessage(callsign + ": not currently sequenced.");
            return;
        }
        const AircraftHoldState& state = it->second;
        const std::string dest = fp.GetFlightPlanData().GetDestination();
        const AirportDemand demand = m_FlowMgr.GetDemand(dest);

        std::ostringstream info;
        info << callsign
             << "  SEQ#" << state.sequencePos
             << "  DELAY +" << state.delayMinutes << "min"
             << "  " << dest
             << " demand:" << CFlowManager::DemandLabel(demand.demandLevel)
             << " (" << demand.holdingCount << "/" << demand.totalInbound << " holding)";
        LogMessage(info.str());
        return;
    }

    if (functionId == TAG_FUNC_DEMAND_INFO)
    {
        EuroScopePlugIn::CFlightPlan fp = FlightPlanSelectASEL();
        if (!fp.IsValid()) return;
        PrintHoldList(fp.GetFlightPlanData().GetDestination());
        return;
    }

    if (functionId == TAG_FUNC_HOLD_MENU)
    {
        EuroScopePlugIn::CFlightPlan fp = FlightPlanSelectASEL();
        if (!fp.IsValid()) return;

        const std::string callsign = fp.GetCallsign();
        auto& state = GetOrCreateState(callsign);

        if (state.candidateFixes.empty())
        {
            LogMessage(callsign + ": no holding fixes found in filed route.");
            return;
        }
        OpenPopupList(area, "Hold Fix", 1);
        for (const auto& fix : state.candidateFixes)
            AddPopupListElement(fix.c_str(), "", TAG_FUNC_HOLD_SELECT, false,
                                EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX);
        AddPopupListElement("-- CLEAR --", "", TAG_FUNC_HOLD_SELECT, false,
                            EuroScopePlugIn::POPUP_ELEMENT_NO_CHECKBOX);
        return;
    }

    if (functionId == TAG_FUNC_HOLD_SELECT)
    {
        EuroScopePlugIn::CFlightPlan fp = FlightPlanSelectASEL();
        if (!fp.IsValid()) return;
        const std::string callsign = fp.GetCallsign();
        const std::string selected  = sItemString ? sItemString : "";
        if (selected == "-- CLEAR --")
        {
            ClearHoldFix(callsign);
            LogMessage(callsign + ": hold cleared.");
        }
        else
        {
            AssignHoldFix(callsign, selected);
        }
        return;
    }
}

// ============================================================
//  OnFlightPlanFlightPlanDataUpdate / OnFlightPlanDisconnect
// ============================================================
void CFMPPlugin::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan fp)
{
    if (!fp.IsValid()) return;
    RefreshAircraft(fp);
}

void CFMPPlugin::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan fp)
{
    if (!fp.IsValid()) return;
    const std::string callsign = fp.GetCallsign();
    const std::string dest     = fp.GetFlightPlanData().GetDestination();
    m_States.erase(callsign);
    m_FlowMgr.RemoveAircraft(callsign);
    if (!dest.empty()) UpdateFlowAndSequence(dest);
}

// ============================================================
//  Private helpers (unchanged from v1.0)
// ============================================================
AircraftHoldState& CFMPPlugin::GetOrCreateState(const std::string& callsign)
{
    return m_States[callsign];
}

void CFMPPlugin::RefreshAircraft(EuroScopePlugIn::CFlightPlan fp)
{
    const std::string callsign = fp.GetCallsign();
    const std::string dest     = fp.GetFlightPlanData().GetDestination();
    const auto& fixes = m_HoldDB.GetFixes(dest);
    if (fixes.empty())
    {
        m_States.erase(callsign);
        m_FlowMgr.RemoveAircraft(callsign);
        return;
    }
    const std::string route = fp.GetFlightPlanData().GetRoute();
    auto& state = GetOrCreateState(callsign);
    const std::string preserved = state.assignedFix;
    state.candidateFixes = m_RouteParser.FindHoldFixes(route, dest, m_HoldDB);
    state.assignedFix    = preserved;
    state.etaUtc         = ParseEta(fp);
    if (!state.assignedFix.empty())
    {
        bool still = std::find(state.candidateFixes.begin(),
                               state.candidateFixes.end(),
                               state.assignedFix) != state.candidateFixes.end();
        if (!still) state.assignedFix.clear();
    }
    AircraftFlowData fd;
    fd.callsign    = callsign;
    fd.destination = dest;
    fd.assignedFix = state.assignedFix;
    fd.isHolding   = !state.assignedFix.empty();
    fd.etaUtc      = state.etaUtc;
    m_FlowMgr.UpdateAircraft(fd);
    UpdateFlowAndSequence(dest);
}

void CFMPPlugin::AssignHoldFix(const std::string& callsign, const std::string& fix)
{
    auto& state = GetOrCreateState(callsign);
    state.assignedFix  = fix;
    state.advisorySent = false;
    EuroScopePlugIn::CFlightPlan fp = FlightPlanSelect(callsign.c_str());
    if (!fp.IsValid()) return;
    const std::string dest = fp.GetFlightPlanData().GetDestination();
    const HoldFix* hf = m_HoldDB.FindFix(dest, fix);
    if (!hf) { LogMessage(callsign + ": fix data not found for " + fix); return; }
    std::string msg = m_Advisory.Build(callsign, dest, *hf);
    if (m_SendAdvisoriesEnabled) {
        // Send the hold advisory to pilot (controller toggleable)
        DisplayUserMessage("FMP", callsign.c_str(), msg.c_str(), true, true, false, false, false);
    }
    state.advisorySent = true;
    LogMessage(callsign + ": advisory prepared -> " + fix +
               " TRK " + std::to_string(hf->inboundTrack) + " deg " + hf->turn + 
               (m_SendAdvisoriesEnabled ? " (sent to pilot)" : " (held back)"));
    AircraftFlowData fd;
    fd.callsign    = callsign;
    fd.destination = dest;
    fd.assignedFix = fix;
    fd.isHolding   = true;
    fd.etaUtc      = state.etaUtc;
    m_FlowMgr.UpdateAircraft(fd);
    UpdateFlowAndSequence(dest);
}

void CFMPPlugin::ClearHoldFix(const std::string& callsign)
{
    auto it = m_States.find(callsign);
    if (it == m_States.end()) return;
    const std::string dest = [&]() -> std::string {
        EuroScopePlugIn::CFlightPlan fp = FlightPlanSelect(callsign.c_str());
        return fp.IsValid() ? fp.GetFlightPlanData().GetDestination() : "";
    }();
    it->second.assignedFix  = "";
    it->second.advisorySent = false;
    it->second.sequencePos  = 0;
    it->second.sequenceTag  = "";
    it->second.delayMinutes = 0;
    AircraftFlowData fd;
    fd.callsign    = callsign;
    fd.destination = dest;
    fd.isHolding   = false;
    fd.etaUtc      = it->second.etaUtc;
    m_FlowMgr.UpdateAircraft(fd);
    if (!dest.empty()) UpdateFlowAndSequence(dest);
}

std::string CFMPPlugin::BuildTagString(const AircraftHoldState& state) const
{
    if (!state.assignedFix.empty())    return state.assignedFix;
    if (!state.candidateFixes.empty()) return "HLD*";
    return "HLD";
}

void CFMPPlugin::LogMessage(const std::string& msg)
{
    DisplayUserMessage("FMP", "Hold Advisor",
                       msg.c_str(), true, true, false, false, false);
}

void CFMPPlugin::ReloadDatabase()
{
    const std::string path = GetDatabasePath();
    if (m_HoldDB.Load(path))
    {
        m_WatchedAirports = m_HoldDB.GetAirports();
        LogMessage("FMP: loaded " + std::to_string(m_WatchedAirports.size()) +
                   " airports from " + path);
    }
    else
    {
        LogMessage("FMP: failed to load database at: " + path);
    }
}

void CFMPPlugin::UpdateFlowAndSequence(const std::string& airport)
{
    auto arrivals = m_FlowMgr.GetAircraftForAirport(airport);
    auto results  = m_SeqMgr.Sequence(arrivals);
    for (const auto& res : results)
    {
        auto it = m_States.find(res.callsign);
        if (it == m_States.end()) continue;
        it->second.sequencePos  = res.position;
        it->second.delayMinutes = res.delayMinutes;
        it->second.sequenceTag  = res.tag;
    }
}

void CFMPPlugin::PrintHoldList(const std::string& airport)
{
    auto demand   = m_FlowMgr.GetDemand(airport);
    auto arrivals = m_FlowMgr.GetAircraftForAirport(airport);
    if (arrivals.empty()) { LogMessage(airport + ": no tracked aircraft."); return; }
    LogMessage("=== " + airport + " | " +
               CFlowManager::DemandLabel(demand.demandLevel) + " demand | " +
               std::to_string(demand.holdingCount) + " holding / " +
               std::to_string(demand.totalInbound) + " inbound ===");
    for (const auto& ac : arrivals)
    {
        auto it = m_States.find(ac.callsign);
        std::string seqStr = "---";
        std::string fixStr = ac.assignedFix.empty() ? "HLD*" : ac.assignedFix;
        if (it != m_States.end() && it->second.sequencePos > 0)
            seqStr = it->second.sequenceTag;
        std::ostringstream line;
        line << std::left << std::setw(9) << ac.callsign
             << std::setw(7) << fixStr << std::setw(8) << seqStr;
        if (ac.etaUtc > 0)
        {
            struct tm t {};
            gmtime_s(&t, &ac.etaUtc);
            char buf[8]; strftime(buf, sizeof(buf), "%H%Mz", &t);
            line << "ETA:" << buf;
        }
        LogMessage(line.str());
    }
}

std::time_t CFMPPlugin::ParseEta(EuroScopePlugIn::CFlightPlan fp)
{
    try
    {
        // GetEnrouteTime returns "HHMM" string in the EuroScope SDK
        const char* enrouteStr = fp.GetFlightPlanData().GetEnrouteHours();
        const char* enrouteMinStr = fp.GetFlightPlanData().GetEnrouteMinutes();
        if (!enrouteStr || enrouteStr[0] == '\0' || !enrouteMinStr || enrouteMinStr[0] == '\0') return 0;
        int enrouteHours = atoi(enrouteStr);
        int enrouteMins = atoi(enrouteMinStr);
        int enrouteMin  = enrouteHours * 60 + enrouteMins;
        if (enrouteMin <= 0) return 0;

        const char* depStr = fp.GetFlightPlanData().GetEstimatedDepartureTime();
        if (!depStr || depStr[0] == '\0') return 0;
        int depHHMM = atoi(depStr);
        if (depHHMM <= 0) return 0;

        std::time_t now = std::time(nullptr);
        struct tm utcNow {};
        gmtime_s(&utcNow, &now);
        struct tm depTm   = utcNow;
        depTm.tm_hour = depHHMM / 100;
        depTm.tm_min  = depHHMM % 100;
        depTm.tm_sec  = 0;
        if (depTm.tm_hour > 23 || depTm.tm_min > 59) return 0; // Invalid HHMM
        // mktime interprets as local time; subtract timezone offset to get UTC epoch
        long tz = 0;
        _get_timezone(&tz);
        std::time_t depUtc = mktime(&depTm) - tz;
        std::time_t etaUtc = depUtc + (enrouteMin * 60);
        if (etaUtc < now - 86400) etaUtc += 86400;
        return etaUtc;
    }
    catch (...) { return 0; }
}

// Toggle implementations
bool CFMPPlugin::IsSendAdvisoriesEnabled() const
{
    return m_SendAdvisoriesEnabled;
}

void CFMPPlugin::SetSendAdvisoriesEnabled(bool enabled)
{
    m_SendAdvisoriesEnabled = enabled;
}

// ============================================================
//  CFMPScreen - constructor
// ============================================================
CFMPScreen::CFMPScreen(CFMPPlugin* plugin)
    : m_Plugin(plugin)
{}

// ============================================================
//  CFMPScreen::OnRefresh  - main paint entry
// ============================================================
void CFMPScreen::OnRefresh(HDC hDC, int phase)
{
    // Draw on the AFTER_LISTS phase so we render on top of aircraft labels
    if (phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS) return;

    // Render panel first (behind button) when visible
    if (m_PanelVisible)
        DrawPanel(hDC);

    // Draw toggle button LAST so it always renders on top of everything
    DrawToggleButton(hDC);
}

// ============================================================
//  DrawToggleButton - small [FMP] button in top-left
// ============================================================
void CFMPScreen::DrawToggleButton(HDC hDC)
{
    COLORREF bg = m_PanelVisible ? RGB(200, 40, 40) : RGB(30, 140, 60);
    RECT btnRect = {m_BtnX, m_BtnY, m_BtnX + BTN_TOG_W, m_BtnY + BTN_TOG_H};
    FillRect(hDC, btnRect, bg);
    FrameRect(hDC, btnRect, CLR_BORDER);
    RECT lblRect = {btnRect.left + 2, btnRect.top, btnRect.right - 2, btnRect.bottom};
    DrawText(hDC, lblRect, "FMP", RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    // Register as a moveable object so it can be dragged; click is handled in OnClickScreenObject
    AddScreenObject(SCREEN_OBJ_BTN_TOGGLE, "fmpbtn", btnRect, true, "");
}

// ============================================================
//  DrawPanel - full FMP panel
// ============================================================
void CFMPScreen::DrawPanel(HDC hDC)
{
    RECT panel{ m_PanelX, m_PanelY,
                m_PanelX + PANEL_W, m_PanelY + PANEL_H };

    // Panel background
    FillRect(hDC, panel, CLR_BG_PANEL);
    FrameRect(hDC, panel, CLR_BORDER);

    // Title bar
    RECT title{ panel.left, panel.top, panel.right, panel.top + TITLE_H };
    DrawTitleBar(hDC, title);

    // Tab bar
    RECT tabs{ panel.left, title.bottom, panel.right, title.bottom + TAB_H };
    DrawTabBar(hDC, tabs);

    // Content area
    RECT content{ panel.left + MARGIN,
                  tabs.bottom + MARGIN,
                  panel.right - MARGIN,
                  panel.bottom - MARGIN };

    switch (m_ActiveTab)
    {
    case 0: DrawStatusTab   (hDC, content); break;
    case 1: DrawAirportsTab (hDC, content); break;
    case 2: DrawDemandTab   (hDC, content); break;
    case 3: DrawDashboardTab(hDC, content); break;
    default: break;
    }
}

// ============================================================
//  DrawTitleBar
// ============================================================
void CFMPScreen::DrawTitleBar(HDC hDC, RECT r)
{
    FillRect(hDC, r, CLR_BG_TITLE);
    RECT lbl{ r.left + 4, r.top, r.right - 40, r.bottom };
    DrawText(hDC, lbl, "FMP Hold Advisor  v1.1");

    // [x] close button on the right
    DrawButton(hDC, r.right - 20, r.top + 1, 18, TITLE_H - 2,
               "X", false, RGB(180,40,40), CLR_TEXT_WHITE,
               SCREEN_OBJ_BTN_TOGGLE, "toggle");

    // Entire title bar is also a drag handle
    AddClickable(SCREEN_OBJ_PANEL_DRAG, "drag", r, /*moveable=*/true);
}

// ============================================================
//  DrawTabBar - Status / Airports / Demand
// ============================================================
void CFMPScreen::DrawTabBar(HDC hDC, RECT r)
{
    int tabW = (r.right - r.left) / 4;

    struct { const char* label; int objId; const char* oid; int idx; } tabs[] = {
        { "Status",    SCREEN_OBJ_TAB_STATUS,    "t0", 0 },
        { "Airports",  SCREEN_OBJ_TAB_AIRPORTS,  "t1", 1 },
        { "Demand",    SCREEN_OBJ_TAB_DEMAND,    "t2", 2 },
        { "Dashboard", SCREEN_OBJ_TAB_DASHBOARD, "t3", 3 },
    };

    for (auto& t : tabs)
    {
        int x = r.left + t.idx * tabW;
        bool active = (m_ActiveTab == t.idx);
        DrawButton(hDC, x, r.top, tabW, TAB_H,
                   t.label, active,
                   active ? CLR_BG_TAB_ON : CLR_BG_TAB_OFF,
                   active ? CLR_TEXT_WHITE : CLR_TEXT_GREY,
                   t.objId, t.oid);
    }
}

// ============================================================
//  DrawStatusTab  - replaces .fmp status + .fmp airports
// ============================================================
void CFMPScreen::DrawStatusTab(HDC hDC, RECT r)
{
    const auto& states   = m_Plugin->GetStates();
    const auto& airports = m_Plugin->GetWatchedAirports();

    int y = r.top;
    auto line = [&](const char* txt, COLORREF c = CLR_TEXT_WHITE)
    {
        RECT lr{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, lr, txt, c);
        y += ROW_H + 1;
    };

    // -- Summary ----------------------------------------------
    char buf[128];
    sprintf_s(buf, "Tracked aircraft : %d", (int)states.size());
    line(buf, CLR_TEXT_GREEN);

    sprintf_s(buf, "Airports in DB   : %d", (int)airports.size());
    line(buf, CLR_TEXT_GREEN);

    bool dbOk = m_Plugin->GetHoldDatabase().IsLoaded();
    line(dbOk ? "Database status  : OK" : "Database status  : NOT LOADED",
         dbOk ? CLR_TEXT_GREEN : CLR_TEXT_RED);

    // Advisory toggle
    bool sendEnabled = m_Plugin->IsSendAdvisoriesEnabled();
    char togTxt[20];
    sprintf_s(togTxt, sizeof(togTxt), "Send Msg: %s", sendEnabled ? "ON" : "OFF");
    RECT togRect{ r.left, y, r.left + 100, y + ROW_H };
    COLORREF togCol = sendEnabled ? CLR_TEXT_GREEN : CLR_TEXT_GREY;
    DrawText(hDC, togRect, togTxt, togCol);
    AddClickable(SCREEN_OBJ_TOGGLE_ADVISORY, "advtoggle", togRect);
    y += ROW_H + 2;

    y += 4;
    line("-----------------------------", CLR_TEXT_GREY);
    line("Airports loaded:", CLR_TEXT_GREY);

    if (airports.empty())
    {
        line("  (none)", CLR_TEXT_GREY);
    }
    else
    {
        // Build a compact comma-separated list, wrapping every ~5
        std::string row;
        int cnt = 0;
        for (const auto& ap : airports)
        {
            if (!row.empty()) row += "  ";
            row += ap;
            if (++cnt % 6 == 0)
            {
                line(("  " + row).c_str());
                row.clear();
            }
        }
        if (!row.empty()) line(("  " + row).c_str());
    }

    // -- Reload DB button at bottom ----------------------------
    int btnY = r.bottom - BTN_TOG_H - 2;
    DrawButton(hDC, r.left, btnY, 90, BTN_TOG_H,
               "Reload DB", false, CLR_BG_BTN, CLR_TEXT_WHITE,
               SCREEN_OBJ_BTN_RELOAD, "reload");
}

// ============================================================
//  DrawAirportsTab  - replaces .fmp list <ICAO>
//  Shows list of active airports; click one to view its aircraft
// ============================================================
void CFMPScreen::DrawAirportsTab(HDC hDC, RECT r)
{
    const auto& airports = m_Plugin->GetWatchedAirports();

    if (!m_SelectedAirport.empty())
    {
        // -- Aircraft list for selected airport --------------
        DrawAircraftListForAirport(hDC, r, m_SelectedAirport);
        return;
    }

    // -- Airport index ----------------------------------------
    int y = r.top;
    RECT hdr{ r.left, y, r.right, y + ROW_H };
    DrawText(hDC, hdr, "Click an airport to view aircraft:", CLR_TEXT_GREY);
    y += ROW_H + 2;

    if (airports.empty())
    {
        RECT lr{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, lr, "  No airports in database.", CLR_TEXT_GREY);
        return;
    }

    const CFlowManager& flow = m_Plugin->GetFlowManager();

    for (int i = 0; i < (int)airports.size(); ++i)
    {
        const std::string& ap = airports[i];
        AirportDemand d = flow.GetDemand(ap);

        char label[64];
        sprintf_s(label, "%-6s  %s  %d/%d",
                  ap.c_str(),
                  CFlowManager::DemandLabel(d.demandLevel).c_str(),
                  d.holdingCount, d.totalInbound);

        RECT rowRect{ r.left, y, r.right, y + ROW_H };
        // Highlight row if it's the hovered/selected airport
        FillRect(hDC, rowRect, RGB(40, 40, 48));

        DrawText(hDC, rowRect, label, DemandColor(d.demandLevel));
        AddClickable(SCREEN_OBJ_AIRPORT_ROW_BASE + i, ap.c_str(), rowRect);
        y += ROW_H + 1;

        if (y + ROW_H > r.bottom) break;
    }
}

// ============================================================
//  DrawAircraftListForAirport  - sub-view inside Airports tab
// ============================================================
void CFMPScreen::DrawAircraftListForAirport(HDC hDC, RECT r,
                                              const std::string& icao)
{
    // Back button
    DrawButton(hDC, r.left, r.top, 50, BTN_TOG_H,
               "< Back", false, CLR_BG_TAB_OFF, CLR_TEXT_WHITE,
               SCREEN_OBJ_AIRPORT_ROW_BASE + 999, "back");

    int y = r.top + BTN_TOG_H + 4;

    const CFlowManager& flow = m_Plugin->GetFlowManager();
    AirportDemand demand = flow.GetDemand(icao);

    char hdr[128];
    sprintf_s(hdr, "%s  %s demand  %d holding / %d inbound",
              icao.c_str(),
              CFlowManager::DemandLabel(demand.demandLevel).c_str(),
              demand.holdingCount, demand.totalInbound);
    RECT hr{ r.left, y, r.right, y + ROW_H };
    DrawText(hDC, hr, hdr, DemandColor(demand.demandLevel));
    y += ROW_H + 2;

    // Column headers
    RECT ch{ r.left, y, r.right, y + ROW_H };
    DrawText(hDC, ch, "CALLSIGN  FIX     SEQ     ETA", CLR_TEXT_GREY);
    y += ROW_H + 1;

    const auto& states = m_Plugin->GetStates();
    auto arrivals = flow.GetAircraftForAirport(icao);

    if (arrivals.empty())
    {
        RECT lr{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, lr, "  No tracked aircraft.", CLR_TEXT_GREY);
        return;
    }

    for (const auto& ac : arrivals)
    {
        if (y + ROW_H > r.bottom) break;

        std::string fixStr = ac.assignedFix.empty() ? "HLD*" : ac.assignedFix;
        std::string seqStr = "---";
        auto it = states.find(ac.callsign);
        if (it != states.end() && it->second.sequencePos > 0)
            seqStr = it->second.sequenceTag;

        std::string etaStr = "----";
        if (ac.etaUtc > 0)
        {
            struct tm t {};
            gmtime_s(&t, &ac.etaUtc);
            char buf[8]; strftime(buf, sizeof(buf), "%H%Mz", &t);
            etaStr = buf;
        }

        char row[80];
        sprintf_s(row, "%-9s %-7s %-7s %s",
                  ac.callsign.c_str(), fixStr.c_str(),
                  seqStr.c_str(), etaStr.c_str());

        COLORREF rowCol = ac.isHolding ? CLR_TEXT_GREEN : CLR_TEXT_WHITE;
        RECT lr{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, lr, row, rowCol);
        y += ROW_H + 1;
    }
}

// ============================================================
//  DrawDemandTab  - replaces .fmp demand
// ============================================================
void CFMPScreen::DrawDemandTab(HDC hDC, RECT r)
{
    const CFlowManager& flow = m_Plugin->GetFlowManager();
    auto active = flow.GetActiveAirports();

    int y = r.top;
    auto line = [&](const char* txt, COLORREF c = CLR_TEXT_WHITE)
    {
        RECT lr{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, lr, txt, c);
        y += ROW_H + 1;
    };

    if (active.empty())
    {
        line("No active airports.", CLR_TEXT_GREY);
    }
    else
    {
        // Column header
        RECT ch{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, ch, "AIRPORT  DEMAND    INBOUND  HOLDING", CLR_TEXT_GREY);
        y += ROW_H + 2;

        for (const auto& ap : active)
        {
            if (y + ROW_H > r.bottom - BTN_TOG_H - 4) break;
            AirportDemand d = flow.GetDemand(ap);
            char row[80];
            sprintf_s(row, "%-8s %-9s %-8d %d",
                      ap.c_str(),
                      CFlowManager::DemandLabel(d.demandLevel).c_str(),
                      d.totalInbound, d.holdingCount);
            RECT lr{ r.left, y, r.right, y + ROW_H };
            DrawText(hDC, lr, row, DemandColor(d.demandLevel));
            y += ROW_H + 1;
        }
    }

    // -- Reload DB button -------------------------------------
    int btnY = r.bottom - BTN_TOG_H - 2;
    DrawButton(hDC, r.left, btnY, 90, BTN_TOG_H,
               "Reload DB", false, CLR_BG_BTN, CLR_TEXT_WHITE,
               SCREEN_OBJ_BTN_RELOAD, "reload");
}

// ============================================================
//  DrawDashboardTab  -- summary overview: demand, holds, totals
// ============================================================
void CFMPScreen::DrawDashboardTab(HDC hDC, RECT r)
{
    const CFlowManager& flow     = m_Plugin->GetFlowManager();
    const auto& states           = m_Plugin->GetStates();
    const auto& airports         = m_Plugin->GetWatchedAirports();
    bool dbOk                    = m_Plugin->GetHoldDatabase().IsLoaded();

    int y = r.top;
    auto line = [&](const char* txt, COLORREF c = CLR_TEXT_WHITE)
    {
        RECT lr{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, lr, txt, c);
        y += ROW_H + 1;
    };

    // -- Header bar -----------------------------------------------
    {
        RECT hdr{ r.left, y, r.right, y + ROW_H };
        FillRect(hDC, hdr, RGB(20, 60, 120));
        DrawText(hDC, hdr, "  FMP DASHBOARD", CLR_TEXT_WHITE,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += ROW_H + 3;
    }

    // -- DB status ------------------------------------------------
    {
        char buf[64];
        sprintf_s(buf, "DB Status : %s", dbOk ? "OK" : "NOT LOADED");
        line(buf, dbOk ? CLR_TEXT_GREEN : CLR_TEXT_RED);
    }

    // -- Totals ---------------------------------------------------
    int totalHolding = 0;
    int totalInbound = 0;
    int highDemandCnt = 0;

    for (const auto& ap : airports)
    {
        AirportDemand d = flow.GetDemand(ap);
        totalHolding += d.holdingCount;
        totalInbound += d.totalInbound;
        if (d.demandLevel >= 2) ++highDemandCnt;
    }

    {
        char buf[64];
        sprintf_s(buf, "Airports  : %d", (int)airports.size());
        line(buf, CLR_TEXT_WHITE);
        sprintf_s(buf, "Tracked AC: %d", (int)states.size());
        line(buf, CLR_TEXT_WHITE);
        sprintf_s(buf, "Holding   : %d", totalHolding);
        line(buf, totalHolding > 0 ? CLR_TEXT_AMBER : CLR_TEXT_GREEN);
        sprintf_s(buf, "Inbound   : %d", totalInbound);
        line(buf, CLR_TEXT_WHITE);
        sprintf_s(buf, "High Demand: %d airport(s)", highDemandCnt);
        line(buf, highDemandCnt > 0 ? CLR_TEXT_RED : CLR_TEXT_GREEN);
    }

    y += 4;
    line("--- High/Medium Demand ---", CLR_TEXT_GREY);

    // -- Per-airport demand rows (level >= 1) ---------------------
    bool anyDemand = false;
    for (const auto& ap : airports)
    {
        if (y + ROW_H > r.bottom - BTN_TOG_H - 4) break;
        AirportDemand d = flow.GetDemand(ap);
        if (d.demandLevel < 1) continue;
        anyDemand = true;

        char row[80];
        sprintf_s(row, "  %-6s  %-6s  %2d holding / %2d inbound",
                  ap.c_str(),
                  CFlowManager::DemandLabel(d.demandLevel).c_str(),
                  d.holdingCount, d.totalInbound);
        RECT lr{ r.left, y, r.right, y + ROW_H };
        DrawText(hDC, lr, row, DemandColor(d.demandLevel));
        y += ROW_H + 1;
    }

    if (!anyDemand)
        line("  All airports: LOW demand", CLR_TEXT_GREEN);

    // -- Reload DB button -----------------------------------------
    int btnY = r.bottom - BTN_TOG_H - 2;
    DrawButton(hDC, r.left, btnY, 90, BTN_TOG_H,
               "Reload DB", false, CLR_BG_BTN, CLR_TEXT_WHITE,
               SCREEN_OBJ_BTN_RELOAD, "reload");
}

// ============================================================
//  DrawButton helper
// ============================================================
RECT CFMPScreen::DrawButton(HDC hDC, int x, int y, int w, int h,
                              const char* label, bool active,
                              COLORREF bg, COLORREF fg,
                              int objectId, const char* oid)
{
    RECT r{ x, y, x + w, y + h };
    FillRect(hDC, r, active ? CLR_BG_BTN_HOT : bg);
    FrameRect(hDC, r, CLR_BORDER);
    RECT lbl{ r.left + 2, r.top, r.right - 2, r.bottom };
    DrawText(hDC, lbl, label, fg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    AddClickable(objectId, oid, r);
    return r;
}

// ============================================================
//  AddClickable - thin wrapper around Add/MoveScreenObject
// ============================================================
void CFMPScreen::AddClickable(int objectId, const char* oid, RECT area,
                               bool moveable)
{
    if (moveable)
        AddScreenObject(objectId, oid, area, true, "");
    else
        AddScreenObject(objectId, oid, area, false, "");
}

// ============================================================
//  OnClickScreenObject
// ============================================================
void CFMPScreen::OnButtonDownScreenObject(int objectType, const char* sObjectId,
                                             POINT pt, RECT /*area*/, int /*button*/)
{
    if (objectType == SCREEN_OBJ_PANEL_DRAG)
    {
        m_PanelDragging = true;
        m_DragOffsetX = pt.x - m_PanelX;
        m_DragOffsetY = pt.y - m_PanelY;
    }
    else if (objectType == SCREEN_OBJ_BTN_TOGGLE && strcmp(sObjectId, "toggle") == 0)
    {
        m_BtnDragging = true;
        m_BtnDragOffsetX = pt.x - m_BtnX;
        m_BtnDragOffsetY = pt.y - m_BtnY;
    }
}

void CFMPScreen::OnButtonUpScreenObject(int objectType, const char* /*sObjectId*/,
                                           POINT /*pt*/, RECT /*area*/, int /*button*/)
{
    if (objectType == SCREEN_OBJ_PANEL_DRAG)
    {
        m_PanelDragging = false;
    }
}

void CFMPScreen::OnClickScreenObject(int objectType, const char* sObjectId,
                                      POINT /*pt*/, RECT /*area*/, int /*button*/)
{
    if (objectType == SCREEN_OBJ_BTN_TOGGLE)
    {
        m_PanelVisible = !m_PanelVisible;
        return;
    }

    if (objectType == SCREEN_OBJ_BTN_RELOAD)
    {
        m_Plugin->ReloadDatabase();
        return;
    }

    if (objectType == SCREEN_OBJ_TAB_STATUS)    { m_ActiveTab = 0; m_SelectedAirport.clear(); return; }
    if (objectType == SCREEN_OBJ_TAB_AIRPORTS)  { m_ActiveTab = 1; m_SelectedAirport.clear(); return; }
    if (objectType == SCREEN_OBJ_TAB_DEMAND)    { m_ActiveTab = 2; m_SelectedAirport.clear(); return; }
    if (objectType == SCREEN_OBJ_TAB_DASHBOARD) { m_ActiveTab = 3; m_SelectedAirport.clear(); return; }

    if (objectType == SCREEN_OBJ_TOGGLE_ADVISORY) {
        m_Plugin->SetSendAdvisoriesEnabled(!m_Plugin->IsSendAdvisoriesEnabled());
        return;
    }

    // Airport row click (Airports tab)
    if (objectType >= SCREEN_OBJ_AIRPORT_ROW_BASE)
    {
        int idx = objectType - SCREEN_OBJ_AIRPORT_ROW_BASE;
        if (idx == 999)
        {
            // Back button
            m_SelectedAirport.clear();
            return;
        }
        // sObjectId holds the ICAO string
        m_SelectedAirport = sObjectId ? sObjectId : "";
        return;
    }
}

// ============================================================
//  OnMoveScreenObject - handle panel drag via title bar
// ============================================================
void CFMPScreen::OnMoveScreenObject(int objectType, const char* /*sObjectId*/,
                                     POINT pt, RECT /*area*/, bool released)
{
    if (objectType == SCREEN_OBJ_PANEL_DRAG && m_PanelDragging)
    {
        m_PanelX = pt.x - m_DragOffsetX;
        m_PanelY = pt.y - m_DragOffsetY;

        if (m_PanelX < 0) m_PanelX = 0;
        if (m_PanelY < 0) m_PanelY = 0;
    }

    if (objectType == SCREEN_OBJ_PANEL_DRAG && released)
    {
        m_PanelDragging = false;
    }

    // [FMP] button drag
    if (objectType == SCREEN_OBJ_BTN_TOGGLE && m_BtnDragging)
    {
        m_BtnX = pt.x - m_BtnDragOffsetX;
        m_BtnY = pt.y - m_BtnDragOffsetY;
        if (m_BtnX < 0) m_BtnX = 0;
        if (m_BtnY < 0) m_BtnY = 0;
    }

    if (objectType == SCREEN_OBJ_BTN_TOGGLE && released)
    {
        m_BtnDragging = false;
    }
}

// ============================================================
//  DLL entry points
// ============================================================
CFMPPlugin *g_pPlugin = nullptr;

void __declspec(dllexport) EuroScopePlugInInit(
    EuroScopePlugIn::CPlugIn **ppPlugInInstance)
{
    *ppPlugInInstance = g_pPlugin = new CFMPPlugin();
}

void __declspec(dllexport) EuroScopePlugInExit()
{
    delete g_pPlugin;
    g_pPlugin = nullptr;
}
