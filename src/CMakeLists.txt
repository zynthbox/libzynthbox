execute_process(
  COMMAND "${Python_EXECUTABLE}" -c "import sys; print(f'{sys.version_info[0]}.{sys.version_info[1]}')"
  OUTPUT_VARIABLE Python_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)

add_library(zynthbox SHARED)

target_sources(zynthbox
    PRIVATE
        plugin.cpp
        ClipAudioSource.cpp
        ClipAudioSourcePositionsModel.cpp
        MidiRouter.cpp
        SamplerSynth.cpp
        SamplerSynthSound.cpp
        SamplerSynthVoice.cpp
        SyncTimer.cpp
        TransportManager.cpp
        Helper.cpp
        QPainterContext.cpp
        WaveFormItem.cpp
        JackPassthrough.cpp
        AudioLevels.cpp
        FilterProxy.cpp
        Note.cpp
        NotesModel.cpp
        MidiRecorder.cpp
        PatternImageProvider.cpp
        PatternModel.cpp
        PlayGrid.cpp
        PlayGridManager.cpp
        SegmentHandler.cpp
        SequenceModel.cpp
        SettingsContainer.cpp
)
target_compile_definitions(zynthbox
    PUBLIC
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_VST3=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCER_ENABLE_GPL_MODE=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_JACK=0
        JUCE_ALSA=1
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    INTERFACE
        $<TARGET_PROPERTY:zynthbox,COMPILE_DEFINITIONS>
)

target_link_libraries(zynthbox
    PRIVATE
        ${RTMIDI_LIBRARIES}
        ${W2GTK4_LIBRARIES}
        ${Jack_LIBRARIES}
        tracktion::tracktion_engine
        tracktion::tracktion_graph
        juce::juce_core
        juce::juce_events
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_gui_extra
        "-latomic"
        "-lcurl"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        Qt5::Core
        Qt5::Gui
        Qt5::Qml
        Qt5::Quick
)

target_include_directories(zynthbox
    PRIVATE
        ${RTMIDI_INCLUDE_DIRS}
        ${Jack_INCLUDE_DIRS}
    INTERFACE
        $<TARGET_PROPERTY:zynthbox,INCLUDE_DIRECTORIES>
)

install(TARGETS zynthbox LIBRARY DESTINATION lib)

# Install zyncoder module to Python dist-packages
install(FILES libzynthbox.py DESTINATION lib/python${Python_VERSION}/dist-packages/)
