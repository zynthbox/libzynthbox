set(TYPESYSTEM_FILE ${CMAKE_CURRENT_SOURCE_DIR}/plugin_typesystem.xml)

add_library(zynthbox SHARED)

target_sources(zynthbox
    PRIVATE
        Plugin.cpp
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

# add_custom_command(TARGET zynthbox POST_BUILD
#     COMMAND ${SHIBOKEN_GENERATOR} --generator-set=shiboken --typesystem-paths=${SHIBOKEN_TYPESYSTEM_DIR} --typesystem-file=${TYPESYSTEM_FILE} --output-directory=${BINDINGS_OUTPUT_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Plugin.h
#     DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Plugin.h
#     COMMENT "Generating Python Bindings"
# )

add_custom_command(TARGET zynthbox POST_BUILD
    COMMAND shiboken2 ${CMAKE_CURRENT_SOURCE_DIR}/Plugin.h ${CMAKE_CURRENT_SOURCE_DIR}/zynthbox.typesystem.xml --output-directory=${BINDINGS_OUTPUT_DIR} --typesystem-paths=/usr/local/lib/python3.7/dist-packages/PySide2/typesystems/ -I/usr/include/arm-linux-gnueabihf/qt5/:/usr/include/arm-linux-gnueabihf/qt5/QtCore:/usr/lib/arm-linux-gnueabihf/qt5//mkspecs/linux-g++:/zynthian/libzynthbox/tracktion_engine/modules/juce/modules/:/zynthian/libzynthbox/tracktion_engine/modules/ --language-level=c++17
    COMMENT "Generating Python Bindings"
)

install(TARGETS zynthbox LIBRARY DESTINATION lib)
install(DIRECTORY ${BINDINGS_OUTPUT_DIR}/ DESTINATION lib/python${Python_VERSION}/site-packages/)

# Install zyncoder module to Python dist-packages
install(FILES libzynthbox.py DESTINATION lib/python${Python_VERSION}/dist-packages/)
