set(TYPESYSTEM_FILE ${CMAKE_CURRENT_SOURCE_DIR}/plugin_typesystem.xml)

add_library(libzynthbox SHARED)

target_sources(libzynthbox
    PRIVATE
        AdjectiveNoun.cpp
        AppImageHelper.cpp
        AudioLevels.cpp
        AudioLevelsChannel.cpp
        AudioTagHelper.cpp
        Chords.cpp
        ClipAudioSource.cpp
        ClipAudioSourcePositionsModel.cpp
        ClipAudioSourcePositionsModelEntry.cpp
        ClipAudioSourceSliceSettings.cpp
        ClipAudioSourceSubvoiceSettings.cpp
        CUIAHelper.cpp
        DiskWriter.cpp
        FifoHandler.cpp
        FilterProxy.cpp
        GainHandler.cpp
        Helper.cpp
        JackConnectionHandler.cpp
        JackPassthrough.cpp
        JackPassthroughAnalyser.cpp
        JackPassthroughCompressor.cpp
        JackPassthroughFilter.cpp
        JackPassthroughVisualiserItem.cpp
        JackThreadAffinitySetter.cpp
        KeyScales.cpp
        MidiRecorder.cpp
        MidiRouter.cpp
        MidiRouterDevice.cpp
        MidiRouterDeviceModel.cpp
        MidiRouterFilter.cpp
        MidiRouterFilterEntry.cpp
        MidiRouterFilterEntryRewriter.cpp
        Note.cpp
        NotesModel.cpp
        PatternImageProvider.cpp
        PatternModel.cpp
        PatternModelVisualiserItem.cpp
        PlayfieldManager.cpp
        PlayGrid.cpp
        PlayGridManager.cpp
        Plugin.cpp
        ProcessWrapper.cpp
        QPainterContext.cpp
        SamplerSynth.cpp
        SamplerSynthSound.cpp
        SamplerSynthVoice.cpp
        SegmentHandler.cpp
        SequenceModel.cpp
        SettingsContainer.cpp
        SketchpadTrackInfo.cpp
        SndCategoryInfo.h
        SndFileInfo.h
        SndLibrary.cpp
        SndLibraryModel.cpp
        SyncTimer.cpp
        SysexHelper.cpp
        SysexIdentity.cpp
        SysexMessage.cpp
        TransportManager.cpp
        WaveFormItem.cpp
        ZynthboxBasics.cpp

        # FolderListModel copy goes here
        folderlistmodel/fileinfothread.cpp
        folderlistmodel/qquickfolderlistmodel.cpp
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
if(PRINT_DEBUG_LOGS)
    target_compile_definitions(libzynthbox PUBLIC DEBUG=1)
else()
    target_compile_definitions(libzynthbox PUBLIC DEBUG=0)
endif()

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
        libappimage
        ${TagLib_LIBRARIES}
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
