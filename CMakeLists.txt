cmake_minimum_required(VERSION 3.22)
project(IRConsole VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
endif()

# JUCE: uses ./JUCE if present, otherwise downloads it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(IRC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND IRC_FORMATS AU)
endif()

juce_add_plugin(IRConsole
    COMPANY_NAME "IRConsole"
    PLUGIN_MANUFACTURER_CODE Ircn
    PLUGIN_CODE Irc8
    FORMATS ${IRC_FORMATS}
    PRODUCT_NAME "IR Console"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(IRConsole PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BlendEngine.cpp
    Source/SpeakerPush.cpp
    Source/ConsoleLookAndFeel.cpp)

target_compile_definitions(IRConsole PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(IRConsole
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(IRC_BUILD_TESTS "Build the headless test app" OFF)
if(IRC_BUILD_TESTS)
    juce_add_console_app(IRConsoleTests PRODUCT_NAME "IRConsoleTests")
    target_sources(IRConsoleTests PRIVATE Tests/Tests.cpp Source/BlendEngine.cpp Source/SpeakerPush.cpp
                   Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/ConsoleLookAndFeel.cpp)
    target_compile_definitions(IRConsoleTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(IRConsoleTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
