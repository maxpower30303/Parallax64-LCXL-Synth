cmake_minimum_required(VERSION 3.22)

project(Parallax64_LCXL_Synth VERSION 1.0.11 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically link the Visual C++ runtime so that neither the standalone nor the VST3 needs the
# Microsoft Visual C++ Redistributable to be installed on the user's machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# RC language for resource files (Windows: embeds the application icon)
if(WIN32)
    enable_language(RC)
endif()

# ==============================================================================
# JUCE (AGPLv3 option). Uses a local checkout if JUCE_DIR is given (CMake
# variable or environment variable), otherwise JUCE 9.0.2 is downloaded.
# ==============================================================================
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout (JUCE is downloaded if empty)")
if(NOT JUCE_DIR AND DEFINED ENV{JUCE_DIR})
    set(JUCE_DIR "$ENV{JUCE_DIR}")
endif()

if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local JUCE: ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    message(STATUS "Downloading JUCE 9.0.2 (set JUCE_DIR to use a local copy)")
    include(FetchContent)
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(Parallax64_LCXL_Synth
    COMPANY_NAME "Flix"
    PLUGIN_MANUFACTURER_CODE Xlsy
    PLUGIN_CODE Px64
    FORMATS VST3 Standalone
    PRODUCT_NAME "Parallax64-LCXL-Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(Parallax64_LCXL_Synth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# JUCE_ASIO=1 uses the ASIO headers that are bundled with JUCE (Steinberg ASIO
# SDK, used here under its GPLv3 option, see THIRD_PARTY_NOTICES.txt). No
# external ASIO SDK is needed or included in this repository.
target_compile_definitions(Parallax64_LCXL_Synth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_ASIO=1)

target_link_libraries(Parallax64_LCXL_Synth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_warning_flags
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# ==============================================================================
# App icon: embeds Source/appicon.ico into the standalone EXE (Windows only).
# ==============================================================================
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/appicon.rc")
    target_sources(Parallax64_LCXL_Synth_Standalone PRIVATE Source/appicon.rc)
    message(STATUS "Embedding the application icon into the standalone EXE.")
endif()
