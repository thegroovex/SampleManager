cmake_minimum_required(VERSION 3.22)

project(SampleManager VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(SM_WITH_LINK  "Build with Ableton Link support (standalone)" ON)
option(SM_BUILD_TESTS "Build the unit test runner" ON)
option(SM_BUILD_PLUGIN "Build the plugin and standalone app" ON)

include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD) # FetchContent_Populate for non-CMake deps
endif()
set(FETCHCONTENT_QUIET ON)

# ---------------------------------------------------------------------------
# Dependencies (all open source). Pin versions for reproducible builds.
# To build offline point FETCHCONTENT_SOURCE_DIR_<NAME> at local checkouts.
# ---------------------------------------------------------------------------
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)

FetchContent_Declare(soundtouch
    GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
    GIT_TAG        f738b1132ec1fd56efc90367898244cf52d9e6a5)

FetchContent_Declare(sqlite
    URL      https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
    URL_HASH SHA256=77823cb110929c2bcb0f5d48e4833b5c59a8a6e40cdea3936b99e199dbbe5784)

FetchContent_MakeAvailable(juce)
FetchContent_GetProperties(soundtouch)
if(NOT soundtouch_POPULATED)
    FetchContent_Populate(soundtouch)
endif()
FetchContent_GetProperties(sqlite)
if(NOT sqlite_POPULATED)
    FetchContent_Populate(sqlite)
endif()

if(SM_WITH_LINK)
    FetchContent_Declare(link
        GIT_REPOSITORY https://github.com/Ableton/link.git
        GIT_TAG        Link-4.1
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES modules/asio-standalone)
    FetchContent_GetProperties(link)
    if(NOT link_POPULATED)
        FetchContent_Populate(link)
    endif()
    include(${link_SOURCE_DIR}/AbletonLinkConfig.cmake)
endif()

# ---------------------------------------------------------------------------
# Third party static libraries
# ---------------------------------------------------------------------------
add_library(sm_sqlite STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
target_include_directories(sm_sqlite PUBLIC ${sqlite_SOURCE_DIR})
target_compile_definitions(sm_sqlite PUBLIC
    SQLITE_THREADSAFE=1 SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_ENABLE_FTS5=1)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(sm_sqlite PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

set(ST_DIR ${soundtouch_SOURCE_DIR}/source/SoundTouch)
add_library(sm_soundtouch STATIC
    ${ST_DIR}/AAFilter.cpp ${ST_DIR}/FIFOSampleBuffer.cpp ${ST_DIR}/FIRFilter.cpp
    ${ST_DIR}/InterpolateCubic.cpp ${ST_DIR}/InterpolateLinear.cpp
    ${ST_DIR}/InterpolateShannon.cpp ${ST_DIR}/RateTransposer.cpp
    ${ST_DIR}/SoundTouch.cpp ${ST_DIR}/TDStretch.cpp ${ST_DIR}/cpu_detect_x86.cpp
    ${ST_DIR}/mmx_optimized.cpp ${ST_DIR}/sse_optimized.cpp ${ST_DIR}/PeakFinder.cpp
    ${ST_DIR}/BPMDetect.cpp)
target_include_directories(sm_soundtouch PUBLIC ${soundtouch_SOURCE_DIR}/include)
target_compile_definitions(sm_soundtouch PUBLIC SOUNDTOUCH_FLOAT_SAMPLES=1)
if(MSVC)
    target_compile_options(sm_soundtouch PRIVATE /O2 /W0)
else()
    target_compile_options(sm_soundtouch PRIVATE -O2 -w)
endif()

# ---------------------------------------------------------------------------
# Shared sources (used by plugin, standalone and tests)
# ---------------------------------------------------------------------------
set(SM_CORE_SOURCES
    Source/Core/Database.cpp
    Source/Core/SearchQuery.cpp
    Source/Core/AudioAnalyzer.cpp
    Source/Core/AutoTagger.cpp
    Source/Core/FilenameParser.cpp
    Source/Core/LibraryScanner.cpp
    Source/Core/MusicTheory.cpp
    Source/Core/AppPaths.cpp
    Source/Audio/SampleRenderer.cpp
    Source/Audio/PreviewEngine.cpp
    Source/Audio/MidiLearn.cpp)

set(SM_JUCE_MODULES
    juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
    juce::juce_gui_extra juce::juce_data_structures)

set(SM_DEFINES
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    SM_VERSION="${PROJECT_VERSION}")

# ---------------------------------------------------------------------------
# Plugin + Standalone
# ---------------------------------------------------------------------------
if(SM_BUILD_PLUGIN)
set(SM_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND SM_FORMATS AU)
endif()

juce_add_plugin(SampleManager
    PRODUCT_NAME                "SampleManager"
    COMPANY_NAME                "SampleManager Open Source"
    BUNDLE_ID                   "io.github.thegroovex.samplemanager"
    PLUGIN_MANUFACTURER_CODE    Tgrx
    PLUGIN_CODE                 Smpm
    FORMATS                     ${SM_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Sampler Tools
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE
    HARDENED_RUNTIME_ENABLED    TRUE)

target_sources(SampleManager PRIVATE
    ${SM_CORE_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Audio/TempoSource.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Translations.cpp
    Source/UI/SearchBar.cpp
    Source/UI/SidePanel.cpp
    Source/UI/SampleTable.cpp
    Source/UI/WaveformView.cpp
    Source/UI/ControlStrip.cpp
    Source/UI/TagEditor.cpp
    Source/UI/SettingsPanel.cpp)

target_include_directories(SampleManager PRIVATE Source)
if(MSVC)
    target_compile_options(SampleManager PRIVATE /utf-8)
endif()
target_compile_definitions(SampleManager PUBLIC ${SM_DEFINES}
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SampleManager
    PRIVATE ${SM_JUCE_MODULES} sm_sqlite sm_soundtouch
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

if(SM_WITH_LINK)
    target_link_libraries(SampleManager PRIVATE Ableton::Link)
    target_compile_definitions(SampleManager PUBLIC SM_WITH_LINK=1)
endif()
endif() # SM_BUILD_PLUGIN

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SM_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(SampleManagerTests PRODUCT_NAME "SampleManagerTests")
    target_sources(SampleManagerTests PRIVATE
        ${SM_CORE_SOURCES}
        Tests/TestMain.cpp
        Tests/AnalyzerTests.cpp
        Tests/TaggerTests.cpp
        Tests/DatabaseTests.cpp
        Tests/RendererTests.cpp
        Tests/TestSignals.cpp)
    target_include_directories(SampleManagerTests PRIVATE Source Tests)
    target_compile_definitions(SampleManagerTests PRIVATE ${SM_DEFINES} JUCE_UNIT_TESTS=1)
    target_link_libraries(SampleManagerTests
        PRIVATE ${SM_JUCE_MODULES} sm_sqlite sm_soundtouch
        PUBLIC juce::juce_recommended_config_flags)
    add_test(NAME unit COMMAND SampleManagerTests)
endif()
