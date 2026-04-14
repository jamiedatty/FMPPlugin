# FMP Hold Advisor — EuroScope Plugin v1.1

A **Flow Management Position** plugin for EuroScope that enhances ATC workflow
by detecting holding fixes in flight plan routes, letting controllers assign
holds via interactive tag items, and automatically sending structured advisory
messages to pilots.

---

## What changed in v1.1

| v1.0 | v1.1 |
|---|---|
| `.fmp list`, `.fmp demand`, `.fmp reload`, `.fmp status`, `.fmp airports` commands | **Removed** — replaced by on-screen GUI panel |
| No on-screen button | **[FMP] button** always visible on every radar screen |
| Functionality only via chat | **Panel with three tabs** (Status / Airports / Demand) |

---

## Features

| Feature | Details |
|---|---|
| **Destination filter** | Only processes aircraft inbound to airports defined in `holds.json` |
| **Route parsing** | Tokenises filed routes, matches waypoints against the hold database |
| **Tag item** | Custom `HLD` / `HLD*` / `FIXNAME` tag column in radar display |
| **Popup menu** | Click the tag to get a dropdown of candidate fixes from the route |
| **Advisory message** | Full structured message sent to pilot on fix assignment |
| **JSON database** | `holds.json` — editable without recompiling the DLL |
| **[FMP] button** | On-screen toggle button (top-left of radar window); click to open/close panel |
| **Status tab** | Shows tracked aircraft count, airports loaded, DB health + [Reload DB] |
| **Airports tab** | Lists every airport in the DB with live demand; click one to view its aircraft |
| **Demand tab** | Per-airport demand level (Low/Medium/High), inbound count, holding count + [Reload DB] |
| **Draggable panel** | Drag the panel title bar to reposition it anywhere on screen |

---

## The [FMP] Panel

```
┌──────────────────────────────────────────────────┐
│  FMP Hold Advisor  v1.1                        [X]│  ← drag to move
├──────────────────────┬──────────────┬─────────────┤
│  Status              │  Airports    │  Demand     │  ← tabs
├──────────────────────────────────────────────────┤
│                                                    │
│  Tracked aircraft : 4                             │
│  Airports in DB   : 3                             │
│  Database status  : OK                            │
│                                                    │
│  Airports loaded:                                 │
│    GMMN  EGLL  LFPG                               │
│                                                    │
│  [Reload DB]                                      │
└──────────────────────────────────────────────────┘
```

### Status tab
Replaces `.fmp status` and `.fmp airports` — shows total tracked aircraft,
number of airports in the database, database load state, and a compact
airport list. A **[Reload DB]** button hot-reloads `holds.json`.

### Airports tab
Replaces `.fmp list <ICAO>` — shows every airport in the DB with live demand
colour-coding (green/amber/red). Click an airport row to drill into a full
aircraft list showing callsign, assigned fix, sequence tag, and ETA.

### Demand tab
Replaces `.fmp demand` — shows all **active** airports (at least one tracked
aircraft) with demand level, total inbound count, and holding count.
Includes a **[Reload DB]** button.

---

## Tag States

```
HLD        Grey    No holding fixes found in the filed route
HLD*       Amber   One or more candidate fixes detected — click to assign
TUPAR      Cyan    Fix assigned; advisory message sent to pilot
```

---

## Build Requirements

| Requirement | Notes |
|---|---|
| **Windows** | MSVC 2019+ or MinGW-w64 with C++17 |
| **CMake** | 3.16+ |
| **EuroScope SDK** | `EuroScopePlugIn.h` from the EuroScope developer package |
| **nlohmann/json** | Auto-downloaded by CMake, or install via `vcpkg install nlohmann-json` |

---

## Build Instructions

### Command line (recommended)

```bat
REM 1. Unzip the plugin source
cd FMPPlugin

REM 2. Configure — point EUROSCOPE_SDK at the folder containing EuroScopePlugIn.h
cmake -B build -DCMAKE_BUILD_TYPE=Release ^
      -DEUROSCOPE_SDK="C:\path\to\EuroScopeSDK"

REM 3. Build
cmake --build build --config Release

REM Output: build\Release\FMPPlugin.dll  and  holds.json (auto-copied)
```

### Visual Studio (GUI)

1. **File → Open → CMake…** → select the `FMPPlugin` folder.
2. In **Project → CMake Settings**, add a CMake variable:
   - Name: `EUROSCOPE_SDK`
   - Value: `C:\path\to\EuroScopeSDK`
3. **Build → Build All** (Release configuration).

### MinGW / MSYS2

```bash
cmake -B build -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DEUROSCOPE_SDK="/c/EuroScopeSDK"
cmake --build build
```

---

## Installation

1. Copy `FMPPlugin.dll` and `holds.json` to your EuroScope plugin folder
   (same directory as `EuroScope.exe` or any sub-folder you prefer).
2. In EuroScope → **Other SET** → **Plug-ins** → **Load** → select `FMPPlugin.dll`.
3. Add the **Hold Advisor** tag item to your radar label layout:
   **Symbology** → **Label Items** → drag `Hold Advisor` into your label string.
4. Add the **Assign Hold Fix** function to the same tag item as the click action.
5. The **[FMP]** button will appear in the top-left of every open radar window.

---

## holds.json Schema

```json
{
  "ICAO": [
    {
      "fix":           "TUPAR",
      "inbound_track": 172,
      "turn":          "RIGHT",
      "alt_ft":        8000,
      "legs_nm":       10,
      "note":          "optional"
    }
  ]
}
```

Add or remove airports and fixes freely — press **[Reload DB]** in the panel
to apply changes without restarting EuroScope.

---

## Advisory Message Format

```
=== TRAFFIC ADVISORY - GMMN ===
TO: BAW234

HIGH TRAFFIC DEMAND — EXPECT HOLDING

HOLDING FIX  : TUPAR
INBOUND TRACK: 172 DEG
TURN DIR     : RIGHT TURNS
MIN ALT      : 8000 FT
LEG LENGTH   : 10 NM

NOTE: THIS INFORMATION MAY CHANGE.
FOLLOW CONTROLLER INSTRUCTIONS AT ALL TIMES.
==========================================
```

---

## Running Tests (No EuroScope SDK Needed)

```bash
g++ -std=c++17 -I include \
    tests/test_main.cpp \
    src/HoldDatabase.cpp \
    src/RouteParser.cpp \
    src/AdvisoryMessage.cpp \
    -o run_tests && ./run_tests
```

Expected output: `35 passed, 0 failed.`

---

## Source Layout

```
FMPPlugin/
├── include/
│   ├── FMPPlugin.h          Core plugin + CFMPScreen GUI class
│   ├── HoldDatabase.h
│   ├── RouteParser.h
│   ├── AdvisoryMessage.h
│   ├── FlowManager.h
│   ├── SequenceManager.h
│   └── json.hpp
├── src/
│   ├── FMPPlugin.cpp        EuroScope callbacks + screen GUI + DLL entry
│   ├── HoldDatabase.cpp
│   ├── RouteParser.cpp
│   ├── AdvisoryMessage.cpp
│   ├── FlowManager.cpp
│   └── SequenceManager.cpp
├── data/
│   └── holds.json
├── tests/
│   └── test_main.cpp
└── CMakeLists.txt
```

---

## License

MIT — free to use, modify, and distribute.
