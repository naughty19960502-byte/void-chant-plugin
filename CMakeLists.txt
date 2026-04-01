cmake_minimum_required(VERSION 3.22)

# =============================================================================
# Project declaration
# =============================================================================
project(VoidChant
    VERSION 1.0.0
    DESCRIPTION "VOID CHANT — Occult Vocal Synthesizer VST3/AU"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Mac Universal Binary (arm64 + x86_64)
# Must be set BEFORE project() on some CMake versions; kept here for clarity.
# The GitHub Actions workflow also passes this via -DCMAKE_OSX_ARCHITECTURES.
# =============================================================================
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
        "Build universal binary for Apple Silicon and Intel" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING
        "Minimum macOS deployment target" FORCE)
endif()

# =============================================================================
# JUCE — fetched via FetchContent so the repo stays self-contained
# =============================================================================
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# =============================================================================
# BinaryData — embed PNG assets at compile time
# =============================================================================
juce_add_binary_data(VoidChantBinaryData
    SOURCES
        Resources/void_chant_bg.png
        Resources/void_chant_knob.png
)

# =============================================================================
# Plugin target
# =============================================================================
juce_add_plugin(VoidChant
    # Identity
    PLUGIN_MANUFACTURER_CODE    Vcnt
    PLUGIN_CODE                 Vcht
    PLUGIN_MANUFACTURER_NAME    "VOID CHANT Audio"
    PRODUCT_NAME                "VOID CHANT"

    # Formats
    FORMATS                     VST3 AU Standalone

    # Plugin type
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Versioning
    VERSION                     1.0.0

    # Bundle IDs (macOS)
    BUNDLE_ID                   com.voidchant.plugin
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice

    # Copy plugin to system folder after build (optional, disable in CI)
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

# =============================================================================
# Source files
# =============================================================================
target_sources(VoidChant
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        # VoidChantVoice.h is header-only; included via PluginProcessor/Editor
)

target_include_directories(VoidChant
    PRIVATE
        Source
)

# =============================================================================
# Link libraries
# =============================================================================
target_link_libraries(VoidChant
    PRIVATE
        VoidChantBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# =============================================================================
# Compile definitions
# =============================================================================
target_compile_definitions(VoidChant
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)

# =============================================================================
# macOS Hardened Runtime entitlements
# Applied by the CI codesign step; referenced here for documentation.
# The entitlements file path: Resources/VoidChant.entitlements
# =============================================================================
if(APPLE)
    set_target_properties(VoidChant PROPERTIES
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
            "${CMAKE_SOURCE_DIR}/Resources/VoidChant.entitlements"
    )
endif()
