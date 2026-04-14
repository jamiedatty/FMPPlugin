cmake_minimum_required(VERSION 3.16)
project(FMPPlugin VERSION 1.0.0 LANGUAGES CXX)

# ── Compiler settings ─────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force 32-bit build for EuroScope compatibility
set(CMAKE_GENERATOR_PLATFORM Win32)
set(CMAKE_SYSTEM_PROCESSOR "x86")

# Explicitly set 32-bit compilers
set(CMAKE_CXX_COMPILER "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x86/cl.exe")
set(CMAKE_C_COMPILER "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x86/cl.exe")
set(CMAKE_LINKER "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x86/link.exe")

# Force 32-bit linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MACHINE:X86")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MACHINE:X86")

# Note: CMAKE_SIZEOF_VOID_P check removed - explicit compiler settings should handle 32-bit build

# ── EuroScope SDK path — set via -DEUROSCOPE_SDK=<path> ───────
set(EUROSCOPE_SDK "" CACHE PATH "Path to EuroScope SDK (contains EuroScopePlugIn.h)")
if(NOT EUROSCOPE_SDK)
    message(WARNING
        "EUROSCOPE_SDK not set. Set it via:\n"
        "  cmake -DEUROSCOPE_SDK=C:/path/to/EuroScopeSDK ..")
endif()

# ── nlohmann/json — already bundled as include/json.hpp ───────
# (No download needed; json.hpp is committed to the repo under include/)

# ── Source files ─────────────────────────────────────────────
set(SOURCES
    src/FMPPlugin.cpp
    src/HoldDatabase.cpp
    src/RouteParser.cpp
    src/AdvisoryMessage.cpp
    src/FlowManager.cpp
    src/SequenceManager.cpp
)

set(HEADERS
    include/FMPPlugin.h
    include/HoldDatabase.h
    include/RouteParser.h
    include/AdvisoryMessage.h
    include/FlowManager.h
    include/SequenceManager.h
)

# ── Build the DLL ─────────────────────────────────────────────
add_library(FMPPlugin SHARED ${SOURCES} ${HEADERS})

target_include_directories(FMPPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EUROSCOPE_SDK}
)

# ── Windows-specific: link Shlwapi (for path helpers) ─────────
if(WIN32)
    target_link_libraries(FMPPlugin PRIVATE Shlwapi ${EUROSCOPE_SDK}/EuroScopePlugInDll.lib)
    target_link_options(FMPPlugin PRIVATE /DELAYLOAD:EuroScope.exe /FORCE:UNRESOLVED)

    # Set DLL output name
    set_target_properties(FMPPlugin PROPERTIES
        OUTPUT_NAME "FMPPlugin"
        PREFIX      ""
        SUFFIX      ".dll"
    )

    # MSVC warnings / optimisation flags
    target_compile_options(FMPPlugin PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX- /MP>
    )
endif()

# ── holds.json: place this file alongside FMPPlugin.dll manually ──
# The plugin reads holds.json from the same folder as the DLL at runtime.
# Do NOT auto-copy it on every build — use your own copy in the deploy folder.
# Install target still copies it once for a clean install:
install(FILES data/holds.json DESTINATION bin)

message(STATUS "FMPPlugin v${PROJECT_VERSION} configured.")
message(STATUS "  EuroScope SDK : ${EUROSCOPE_SDK}")
message(STATUS "  Build type    : ${CMAKE_BUILD_TYPE}")
