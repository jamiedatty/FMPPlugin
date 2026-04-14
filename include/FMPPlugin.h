#pragma once
// ============================================================
//  FMPPlugin.h  -  Flow Management Position Plugin for EuroScope
//  MVP v1.1
//
//  Changes from v1.0:
//    - Removed all .fmp chat commands (OnCompileCommand deleted)
//    - Added CFMPScreen : CRadarScreen with a draggable on-screen
//      GUI panel (toggled by a persistent [FMP] button).
//      The panel provides three tabs replicating every feature
//      the removed commands offered:
//        Status   - tracked aircraft count + loaded airports count
//        Airports - list of DB airports; click one to see its
//                   aircraft list
//        Demand   - demand level + hold/inbound counts for every
//                   active airport; includes a [Reload DB] button
// ============================================================
#include <Windows.h>
#include "EuroScopePlugIn.h"
#include "HoldDatabase.h"
#include "RouteParser.h"
#include "AdvisoryMessage.h"
#include "FlowManager.h"
#include "SequenceManager.h"

#include <string>
#include <unordered_map>
#include <vector>

// -- Tag item / function IDs ----------------------------------
static constexpr int TAG_ITEM_HOLD_STATUS   = 900;
static constexpr int TAG_FUNC_HOLD_MENU     = 901;
static constexpr int TAG_FUNC_HOLD_SELECT   = 902;
static constexpr int TAG_ITEM_SEQ_STATUS    = 903;
static constexpr int TAG_FUNC_SEQ_MENU      = 904;
static constexpr int TAG_FUNC_DEMAND_INFO   = 905;
static constexpr int TAG_ITEM_HOLD_DETAILS  = 910;

// -- Screen object IDs ----------------------------------------
static constexpr int SCREEN_OBJ_BTN_TOGGLE   = 1000;  // [FMP] toggle
static constexpr int SCREEN_OBJ_PANEL_DRAG   = 1001;  // panel title bar drag
static constexpr int SCREEN_OBJ_BTN_RELOAD   = 1002;  // [Reload DB]
static constexpr int SCREEN_OBJ_TAB_STATUS   = 1003;
static constexpr int SCREEN_OBJ_TAB_AIRPORTS = 1004;
static constexpr int SCREEN_OBJ_TAB_DEMAND   = 1005;
// Airport row clicks: SCREEN_OBJ_AIRPORT_ROW_BASE + row index (0-based)
static constexpr int SCREEN_OBJ_AIRPORT_ROW_BASE = 1010;
static constexpr int SCREEN_OBJ_TOGGLE_ADVISORY  = 1016;
static constexpr int SCREEN_OBJ_TAB_DASHBOARD = 1017;

// -- Plugin metadata ------------------------------------------
static constexpr char PLUGIN_NAME[]      = "FMP Hold Advisor";
static constexpr char PLUGIN_VERSION[]   = "1.1.2";
static constexpr char PLUGIN_AUTHOR[]    = "FMP Plugin";
static constexpr char PLUGIN_COPYRIGHT[] = "Open Source";

// -- Per-aircraft hold state ----------------------------------
struct AircraftHoldState
{
    std::vector<std::string> candidateFixes;
    std::string              assignedFix;
    bool                     advisorySent  = false;
    int                      sequencePos   = 0;
    int                      delayMinutes  = 0;
    std::string              sequenceTag;
    std::time_t              etaUtc        = 0;
};

// -- Forward declaration --------------------------------------
class CFMPPlugin;

// ============================================================
//  CFMPScreen  -  one radar-screen instance; owns the GUI panel
// ============================================================
class CFMPScreen : public EuroScopePlugIn::CRadarScreen
{
public:
    explicit CFMPScreen(CFMPPlugin* plugin);
    virtual ~CFMPScreen() = default;

    // CRadarScreen overrides
    void OnRefresh(HDC hDC, int phase) override;
    void OnButtonDownScreenObject(int objectType, const char* sObjectId,
                                   POINT pt, RECT area, int button) override;
    void OnButtonUpScreenObject(int objectType, const char* sObjectId,
                                 POINT pt, RECT area, int button) override;
    void OnClickScreenObject(int objectType, const char* sObjectId,
                             POINT pt, RECT area, int button) override;
    void OnMoveScreenObject(int objectType, const char* sObjectId,
                            POINT pt, RECT area, bool released) override;
    // Required pure virtual - called when the ASR is being closed
    void OnAsrContentToBeClosed() override {}

private:
    CFMPPlugin* m_Plugin;           // back-pointer (not owned)

    // Panel state
    bool        m_PanelVisible   = false;
    int         m_ActiveTab      = 0;   // 0=Status 1=Airports 2=Demand
    std::string m_SelectedAirport;      // populated when user clicks an airport row

    // Panel position (draggable via title bar)
    int   m_PanelX = 10;
    int   m_PanelY = 40;
    bool  m_PanelDragging = false;
    int   m_DragOffsetX = 0;
    int   m_DragOffsetY = 0;

    // FMP button draggable
    int   m_BtnX = 10;
    int   m_BtnY = 10;
    bool  m_BtnDragging = false;
    int   m_BtnDragOffsetX = 0;
    int   m_BtnDragOffsetY = 0;

    // -- Panel dimensions -------------------------------------
    static constexpr int PANEL_W  = 350;
    static constexpr int PANEL_H  = 360;
    static constexpr int TITLE_H  = 18;
    static constexpr int TAB_H    = 20;
    static constexpr int ROW_H    = 15;
    static constexpr int MARGIN   =  6;
    static constexpr int BTN_TOG_W = 50;
    static constexpr int BTN_TOG_H = 25;

    // -- Drawing helpers --------------------------------------
    void DrawToggleButton(HDC hDC);
    void DrawPanel(HDC hDC);
    void DrawTitleBar(HDC hDC, RECT r);
    void DrawTabBar(HDC hDC, RECT r);
    void DrawStatusTab(HDC hDC, RECT r);
    void DrawAirportsTab(HDC hDC, RECT r);
    void DrawDemandTab(HDC hDC, RECT r);
    void DrawDashboardTab(HDC hDC, RECT r);
    void DrawAircraftListForAirport(HDC hDC, RECT r, const std::string& icao);

    // Draw a filled rectangle button; registers it as a screen object and
    // returns the RECT used so callers can stack items.
    RECT DrawButton(HDC hDC, int x, int y, int w, int h,
                    const char* label, bool active,
                    COLORREF bg, COLORREF fg, int objectId, const char* oid);

    // Thin wrapper: registers rect as draggable or clickable screen object
    void AddClickable(int objectId, const char* oid, RECT area, bool moveable = false);
};

// ============================================================
//  CFMPPlugin
// ============================================================
class CFMPPlugin : public EuroScopePlugIn::CPlugIn
{
public:
    CFMPPlugin();
    virtual ~CFMPPlugin();

    // -- EuroScope overrides ----------------------------------
    void OnGetTagItem(
        EuroScopePlugIn::CFlightPlan   fp,
        EuroScopePlugIn::CRadarTarget  rt,
        int                            itemCode,
        int                            tagData,
        char                          *sItemString,
        int                           *pColorCode,
        COLORREF                      *pRGB,
        double                        *pFontSize) override;

    void OnFunctionCall(
        int                            functionId,
        const char                    *sItemString,
        POINT                          pt,
        RECT                           area) override;

    void OnFlightPlanFlightPlanDataUpdate(
        EuroScopePlugIn::CFlightPlan   fp) override;

    void OnFlightPlanDisconnect(
        EuroScopePlugIn::CFlightPlan   fp) override;

    // NOTE: OnCompileCommand has been intentionally removed.
    // All formerly .fmp-command functionality is available in
    // the on-screen FMP panel opened with the [FMP] button.

    EuroScopePlugIn::CRadarScreen* OnRadarScreenCreated(
        const char* sDisplayName,
        bool        NeedRadarContent,
        bool        GeoReferenced,
        bool        CanBeSaved,
        bool        CanBeCreated) override;

    // -- Public accessors used by CFMPScreen ------------------
    void                            ReloadDatabase();
    const CFlowManager&             GetFlowManager()     const { return m_FlowMgr;  }
    const CHoldDatabase&            GetHoldDatabase()    const { return m_HoldDB;   }
    const std::unordered_map<std::string, AircraftHoldState>&
                                    GetStates()          const { return m_States;   }
    const std::vector<std::string>& GetWatchedAirports() const { return m_WatchedAirports; }
    void                            LogMessage(const std::string& msg);

    // Toggle: send pilot advisories (DisplayUserMessage)
    bool                            IsSendAdvisoriesEnabled() const;
    void                            SetSendAdvisoriesEnabled(bool enabled);

private:
    // -- Subsystems -------------------------------------------
    CHoldDatabase    m_HoldDB;
    bool             m_SendAdvisoriesEnabled = true;
    CRouteParser     m_RouteParser;
    CAdvisoryMessage m_Advisory;
    CFlowManager     m_FlowMgr;
    CSequenceManager m_SeqMgr;

    std::unordered_map<std::string, AircraftHoldState> m_States;
    std::vector<std::string>                            m_WatchedAirports;

    // Keep non-owning pointers so plugin can request redraws in the future
    std::vector<CFMPScreen*> m_Screens;

    // -- Private helpers --------------------------------------
    AircraftHoldState& GetOrCreateState(const std::string& callsign);
    void               RefreshAircraft(EuroScopePlugIn::CFlightPlan fp);
    void               AssignHoldFix(const std::string& callsign,
                                     const std::string& fix);
    void               ClearHoldFix(const std::string& callsign);
    std::string        BuildTagString(const AircraftHoldState& state) const;
    void               UpdateFlowAndSequence(const std::string& airport);
    void               PrintHoldList(const std::string& airport);
    static std::time_t ParseEta(EuroScopePlugIn::CFlightPlan fp);
};

// ── DLL entry points ──────────────────────────────────────────
// extern "C" is NOT needed here — EuroScopePlugIn.h already
// declares these with C linkage internally. Adding it again
// causes C2732 "linkage specification contradicts earlier spec".
void __declspec(dllexport) EuroScopePlugInInit(
    EuroScopePlugIn::CPlugIn **ppPlugInInstance);
void __declspec(dllexport) EuroScopePlugInExit();
