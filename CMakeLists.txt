cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Bodenleger VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(BL_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(BL_FORMATS VST3 Standalone)
else()
    set(BL_FORMATS VST3)
endif()

juce_add_plugin(Bodenleger
    COMPANY_NAME "Bodenleger"
    BUNDLE_ID "com.humio1.bodenleger"
    PLUGIN_MANUFACTURER_CODE Hmio
    PLUGIN_CODE Bdlg
    FORMATS ${BL_FORMATS}
    PRODUCT_NAME "Bodenleger"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Generator
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(BodenlegerData SOURCES
    Resources/fonts/Yellowtail-Regular.ttf
    Resources/fonts/Oswald-Variable.ttf
    Resources/fonts/SpecialElite-Regular.ttf
    Resources/fonts/CourierPrime-Bold.ttf)

target_sources(Bodenleger PRIVATE
    Source/Engine/Spectral.cpp
    Source/Engine/Engine.cpp
    Source/Engine/Builder.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/BenchLookAndFeel.cpp
    Source/UI/Components.cpp)

target_include_directories(Bodenleger PRIVATE Source)

target_compile_definitions(Bodenleger PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(UNIX AND NOT APPLE)
    # Lets the plugin compile on a bare Linux machine for quick checks.
    target_compile_definitions(Bodenleger PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

set(BL_JUCE_MODULES juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_extra)
if(APPLE OR WIN32)
    list(APPEND BL_JUCE_MODULES juce::juce_audio_utils)
endif()

target_link_libraries(Bodenleger
    PRIVATE
        BodenlegerData
        ${BL_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline engine test (no JUCE needed)
add_executable(EngineTest tests/EngineTest.cpp Source/Engine/Spectral.cpp Source/Engine/Engine.cpp Source/Engine/Builder.cpp)
target_include_directories(EngineTest PRIVATE Source)

# Developer tool: render the editor to a PNG without a host (off by default)
option(BL_DEV_TOOLS "Build developer tools" OFF)
if(BL_DEV_TOOLS)
    add_executable(EditorSnapshot tests/EditorSnapshot.cpp)
    target_link_libraries(EditorSnapshot PRIVATE Bodenleger)
    target_include_directories(EditorSnapshot PRIVATE Source ${JUCE_SOURCE_DIR}/modules
        ${CMAKE_BINARY_DIR}/Bodenleger_artefacts/JuceLibraryCode)
    get_target_property(BL_DEFS Bodenleger COMPILE_DEFINITIONS)
    target_compile_definitions(EditorSnapshot PRIVATE ${BL_DEFS})
endif()
