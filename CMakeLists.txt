cmake_minimum_required(VERSION 3.22)

project(AssEffect VERSION 1.0.0 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Ass Effect is exclusively supported on Linux")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ASS_EFFECT_NATIVE_OPTIMIZATIONS "Optimise for the current CPU" OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    message(STATUS "Local JUCE checkout not found; fetching JUCE 8.0.10")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(AssEffect
    COMPANY_NAME "Wamphyre"
    BUNDLE_ID "audio.wamphyre.asseffect"
    PLUGIN_MANUFACTURER_CODE Wamp
    PLUGIN_CODE AsEf
    FORMATS VST3
    PRODUCT_NAME "Ass Effect"
    DESCRIPTION "Cassette, vinyl and underground lo-fi processor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
)

juce_generate_juce_header(AssEffect)

target_sources(AssEffect PRIVATE
    Source/FactoryPresets.h
    Source/LoFiEngine.cpp
    Source/LoFiEngine.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
)

target_compile_definitions(AssEffect PUBLIC
    JUCE_REPORT_APP_USAGE=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AssEffect
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(ASS_EFFECT_NATIVE_OPTIMIZATIONS)
    if(MSVC)
        target_compile_options(AssEffect PRIVATE /arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(AssEffect PRIVATE -march=native -mtune=native)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(AssEffect PRIVATE $<$<CONFIG:Release>:-O3>)
endif()

include(CTest)

if(BUILD_TESTING)
    juce_add_console_app(AssEffectDSPTests PRODUCT_NAME "Ass Effect DSP Tests")
    juce_generate_juce_header(AssEffectDSPTests)
    target_sources(AssEffectDSPTests PRIVATE
        Tests/DSPTests.cpp
        Source/FactoryPresets.h
        Source/LoFiEngine.cpp
        Source/LoFiEngine.h
    )
    target_include_directories(AssEffectDSPTests PRIVATE Source)
    target_compile_definitions(AssEffectDSPTests PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
    )
    target_link_libraries(AssEffectDSPTests PRIVATE
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME AssEffectDSP COMMAND AssEffectDSPTests)
endif()
