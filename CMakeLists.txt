cmake_minimum_required(VERSION 3.22)

project(VoodooKilla VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(VK_BUILD_TESTS "Build unit / offline render tests" ON)

# ---------------------------------------------------------------------------
# JUCE (pinned tag). For offline builds pass -DFETCHCONTENT_SOURCE_DIR_JUCE=<path>.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Resources -> BinaryData (fixed file names, assets can be swapped without code changes)
# ---------------------------------------------------------------------------
file(GLOB VK_RESOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Presets/*.json
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Photos/*.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Photos/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Textures/*.png
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Textures/*.jpg
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/*.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/*.otf
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Logo/*.png)

juce_add_binary_data(VoodooKillaData
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES     ${VK_RESOURCE_FILES})

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
file(GLOB VK_ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/engine/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/engine/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/engine/Modules/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/engine/Modules/*.h)

file(GLOB VK_UI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/ui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/ui/*.h)

set(VK_PLUGIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginEditor.h)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(VK_WARNING_TWEAKS -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-missing-field-initializers)
elseif(MSVC)
    set(VK_WARNING_TWEAKS /W4 /wd4100)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(VK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VK_FORMATS AU)
endif()

juce_add_plugin(VoodooKilla
    COMPANY_NAME              "Killa"
    PRODUCT_NAME              "Voodoo Killa"
    BUNDLE_ID                 com.killa.voodookilla
    PLUGIN_MANUFACTURER_CODE  Klla
    PLUGIN_CODE               Vdkl
    FORMATS                   ${VK_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Delay Modulation
    AU_MAIN_TYPE              kAudioUnitType_MusicEffect
    DESCRIPTION               "Cast a preset. Kill the melody.")

target_sources(VoodooKilla PRIVATE ${VK_PLUGIN_SOURCES} ${VK_ENGINE_SOURCES} ${VK_UI_SOURCES})
target_include_directories(VoodooKilla PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_options(VoodooKilla PRIVATE ${VK_WARNING_TWEAKS})

target_compile_definitions(VoodooKilla PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VoodooKilla
    PRIVATE
        VoodooKillaData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# ---------------------------------------------------------------------------
# Tests (engine only, no plugin wrapper)
# ---------------------------------------------------------------------------
if(VK_BUILD_TESTS)
    juce_add_console_app(VoodooKillaTests PRODUCT_NAME "VoodooKillaTests")
    file(GLOB VK_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp)
    target_sources(VoodooKillaTests PRIVATE ${VK_TEST_SOURCES} ${VK_ENGINE_SOURCES})
    target_include_directories(VoodooKillaTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_options(VoodooKillaTests PRIVATE ${VK_WARNING_TWEAKS})
    target_compile_definitions(VoodooKillaTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_UNIT_TESTS=0)
    target_link_libraries(VoodooKillaTests
        PRIVATE
            VoodooKillaData
            juce::juce_audio_basics
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags)

    enable_testing()
    add_test(NAME VoodooKillaTests COMMAND VoodooKillaTests)
endif()

# ---------------------------------------------------------------------------
# UI snapshot tool (not built by default): renders the editor to a PNG for design checks.
#   cmake --build build --target VoodooKillaSnapshot && ./VoodooKillaSnapshot out.png [scale]
# ---------------------------------------------------------------------------
juce_add_console_app(VoodooKillaSnapshot PRODUCT_NAME "VoodooKillaSnapshot")
set_target_properties(VoodooKillaSnapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(VoodooKillaSnapshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Tools/Snapshot.cpp
    ${VK_PLUGIN_SOURCES} ${VK_ENGINE_SOURCES} ${VK_UI_SOURCES})
target_include_directories(VoodooKillaSnapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_options(VoodooKillaSnapshot PRIVATE ${VK_WARNING_TWEAKS})
target_compile_definitions(VoodooKillaSnapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="Voodoo Killa" JucePlugin_VersionString="${PROJECT_VERSION}")
target_link_libraries(VoodooKillaSnapshot PRIVATE
    VoodooKillaData juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags)
