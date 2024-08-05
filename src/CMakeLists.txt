set(TYPESYSTEM_FILE ${CMAKE_CURRENT_SOURCE_DIR}/plugin_typesystem.xml)

add_library(libzynthbox SHARED)

target_sources(libzynthbox
    PRIVATE
        Plugin.cpp
        ClipAudioSource.cpp
        ClipAudioSourceSubvoiceSettings.cpp
        ClipAudioSourcePositionsModel.cpp
        ClipAudioSourcePositionsModelEntry.cpp
        MidiRouter.cpp
        MidiRouterDevice.cpp
        SamplerSynth.cpp
        SamplerSynthSound.cpp
        SamplerSynthVoice.cpp
        SyncTimer.cpp
        TransportManager.cpp
        Helper.cpp
        QPainterContext.cpp
        WaveFormItem.cpp
        JackPassthrough.cpp
        JackPassthroughAnalyser.cpp
        JackPassthroughCompressor.cpp
        JackPassthroughVisualiserItem.cpp
        JackPassthroughFilter.cpp
        JackThreadAffinitySetter.cpp
        AudioLevels.cpp
        AudioLevelsChannel.cpp
        DiskWriter.cpp
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
        PlayfieldManager.cpp
        MidiRouterDeviceModel.cpp
        KeyScales.cpp
        Chords.cpp
        ProcessWrapper.cpp
        AdjectiveNoun.cpp
)
target_compile_definitions(libzynthbox
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
        JUCE_DSP_USE_SHARED_FFTW=1
        JUCE_JACK=0
        JUCE_ALSA=1
        TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    INTERFACE
        $<TARGET_PROPERTY:libzynthbox,COMPILE_DEFINITIONS>
)

target_link_libraries(libzynthbox
    PRIVATE
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
        "-lfftw3"
    PUBLIC
        OpenMP::OpenMP_CXX
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        Qt5::Core
        Qt5::Gui
        Qt5::Qml
        Qt5::Quick
        KF5::Pty
)

target_include_directories(libzynthbox
    PRIVATE
        ${Jack_INCLUDE_DIRS}
        /usr/include/arm-linux-gnueabihf/qt5/QtQml/5.15.8
        /usr/include/arm-linux-gnueabihf/qt5/QtQml/5.15.8/QtQml
        /usr/include/arm-linux-gnueabihf/qt5/QtCore/5.15.8
        /usr/include/arm-linux-gnueabihf/qt5/QtCore/5.15.8/QtCore
    INTERFACE
        $<TARGET_PROPERTY:libzynthbox,INCLUDE_DIRECTORIES>
)

set_property(TARGET libzynthbox PROPERTY PREFIX "")

install(
    TARGETS libzynthbox
    LIBRARY DESTINATION lib
)
