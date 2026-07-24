cmake_minimum_required(VERSION 3.22)

project(MultibandConvolver VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MultibandConvolver
    COMPANY_NAME "Mirror Maze"
    PLUGIN_MANUFACTURER_CODE Izak
    PLUGIN_CODE Mbcv
    FORMATS VST3 Standalone
    PRODUCT_NAME "Multiband Convolver"
    VST3_CATEGORIES Fx Reverb
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(MultibandConvolver
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/CrossoverSplitter.cpp
        Source/DSP/BandChain.cpp
        Source/DSP/IRLibrary.cpp
        Source/DSP/IRProcessor.cpp
        Source/DSP/IRReshapeWorker.cpp
        Source/Params/ParameterLayout.cpp
        Source/GUI/SpectrumBandStrip.cpp
        Source/GUI/MacroPanelComponent.cpp
        Source/GUI/LookAndFeelSaturnish.cpp
        Source/GUI/KnobPopup.cpp
)

target_include_directories(MultibandConvolver
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(MultibandConvolver
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

juce_add_binary_data(MultibandConvolverTextures SOURCES
    Resources/Textures/knob_metal.jpg
    Resources/Textures/chassis_metal.jpg
    Resources/Fonts/Jost-Regular.ttf
    Resources/Fonts/Jost-SemiBold.ttf
)

target_link_libraries(MultibandConvolver
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        MultibandConvolverTextures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Lightweight offline DSP verification harness -- deliberately excludes GUI/plugin-client modules
# (CrossoverSplitter/BandChain/IRLibrary have no GUI dependency) so it builds fast and can run in
# any environment, no audio device or DAW host required.
add_executable(OfflineDspTest
    Tests/OfflineDspTest.cpp
    Source/DSP/CrossoverSplitter.cpp
    Source/DSP/BandChain.cpp
    Source/DSP/IRLibrary.cpp
    Source/DSP/IRProcessor.cpp
    Source/DSP/IRReshapeWorker.cpp
)

target_include_directories(OfflineDspTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_link_libraries(OfflineDspTest
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
