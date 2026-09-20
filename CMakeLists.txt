cmake_minimum_required(VERSION 3.22)

project(DrawSynth VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# -----------------------------------------------------------------------------
# JUCE. Point -DJUCE_PATH=/path/to/JUCE at an existing local checkout to reuse
# it (much faster for repeat builds); otherwise CMake fetches JUCE 8.0.7
# automatically on first configure - genuinely a two-command build with
# nothing to install by hand beyond a compiler/CMake and JUCE's usual Linux
# dependencies (see README.md).
# -----------------------------------------------------------------------------
if (DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.7
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# -----------------------------------------------------------------------------
# The plugin target. FORMATS is VST3 + Standalone: JUCE has no native CLAP
# format (as of 8.0.7) - see the CLAP section below for how to add one.
# -----------------------------------------------------------------------------

# Auto-copying the freshly built plugin into the host machine's own VST3
# folder only makes sense when the build's target platform IS the host -
# never when cross-compiling (e.g. building a Windows .vst3 from Linux to
# package into an installer), where the "copy to Program Files" step would
# just create a nonsense path on the build machine.
if (CMAKE_CROSSCOMPILING)
    set(DRAWSYNTH_COPY_AFTER_BUILD FALSE)
else()
    set(DRAWSYNTH_COPY_AFTER_BUILD TRUE)
endif()

juce_add_plugin(DrawSynth
    VERSION                     1.0.0
    COMPANY_NAME                "DrawSynth Project"
    BUNDLE_ID                   "com.drawsynth-project.drawsynth"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${DRAWSYNTH_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE    Dswx
    PLUGIN_CODE                 Dsy1
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "DrawSynth")

juce_generate_juce_header(DrawSynth)

target_sources(DrawSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Music/Scale.cpp
    Source/Music/ScaleQuantizer.cpp
    Source/Canvas/StrokeModel.cpp
    Source/Canvas/NoteTimelineBuilder.cpp
    Source/Audio/SynthVoice.cpp
    Source/Audio/SynthEngine.cpp
    Source/Midi/MidiExporter.cpp
    Source/State/StrokeSerialization.cpp
    Source/State/PresetManager.cpp
    Source/UI/CanvasComponent.cpp
    Source/UI/TopControlBar.cpp
    Source/UI/PresetBar.cpp
    Source/UI/DragExportButton.cpp
    Source/UI/TransportBar.cpp)

target_include_directories(DrawSynth PRIVATE Source)

target_compile_definitions(DrawSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DrawSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_data_structures
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -----------------------------------------------------------------------------
# Optional CLAP target via the community clap-juce-extensions project
# (https://github.com/free-audio/clap-juce-extensions). JUCE itself does not
# build CLAP plugins, so this is off by default. To turn it on:
#
#   git submodule add https://github.com/free-audio/clap-juce-extensions.git libs/clap-juce-extensions
#   git submodule update --init --recursive
#   cmake -B build -DDRAWSYNTH_BUILD_CLAP=ON
#
# See README.md - "Building the CLAP target" for more detail.
# -----------------------------------------------------------------------------
option(DRAWSYNTH_BUILD_CLAP "Also build a CLAP target via clap-juce-extensions" OFF)

if(DRAWSYNTH_BUILD_CLAP)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/clap-juce-extensions/CMakeLists.txt")
        message(FATAL_ERROR
            "DRAWSYNTH_BUILD_CLAP is ON but libs/clap-juce-extensions is missing or empty. "
            "See README.md - 'Building the CLAP target' for the git submodule commands.")
    endif()

    add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

    clap_juce_extensions_plugin(TARGET DrawSynth
        CLAP_ID "com.drawsynth-project.drawsynth"
        CLAP_FEATURES instrument synthesizer)
endif()
