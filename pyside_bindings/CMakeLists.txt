set(bindings_library "Zynthbox")
set(bindings_header ${CMAKE_CURRENT_SOURCE_DIR}/bindings.h)
set(bindings_typesystem ${CMAKE_CURRENT_SOURCE_DIR}/bindings.xml)
set(generated_sources
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/audiolevels_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/clipaudiosource_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/clipaudiosourcesubvoicesettings_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/cuiahelper_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/gainhandler_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/jackconnectionhandler_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/jackpassthrough_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/jackpassthroughcompressor_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/jackpassthroughfilter_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/keyscales_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/midirouter_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/midirouterfilter_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/midirouterfilterentry_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/midirouterfilterentryrewriter_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/midirouterdevice_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/midirouterdevicemodel_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/midirecorder_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/notesmodel_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/patternmodel_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/playgridmanager_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/playfieldmanager_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/plugin_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/processwrapper_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/sequencemodel_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/synctimer_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/zynthboxbasics_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/zynthbox_module_wrapper.cpp
)

# Add custom target to run shiboken to generate the binding cpp files.
add_custom_command(
    OUTPUT ${generated_sources}
    COMMAND Shiboken2::shiboken2
                # --debug-level=full
                --generator-set=shiboken
                --enable-parent-ctor-heuristic
                # --enable-return-value-heuristic
                --enable-pyside-extensions
                --use-isnull-as-nb_nonzero
                --language-level=c++20
                --output-directory=${CMAKE_CURRENT_BINARY_DIR}
                --typesystem-paths=${PySide2_TYPESYSTEMS_PATH}
                "-I$<TARGET_PROPERTY:libzynthbox,SOURCE_DIR>"
                "-I$<JOIN:$<TARGET_PROPERTY:libzynthbox,INCLUDE_DIRECTORIES>,;-I>"
                "-I$<JOIN:${Shiboken2_INCLUDE_DIRS},;-I>"
                ${bindings_header}
                ${bindings_typesystem}
    COMMAND_EXPAND_LISTS
    DEPENDS ${bindings_header} ${bindings_typesystem}
    IMPLICIT_DEPENDS CXX ${bindings_header}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running generator for ${bindings_typesystem}."
)

# Define and build the bindings library.
add_library(${bindings_library} MODULE ${generated_sources})

# Apply relevant include and link flags.
target_include_directories(${bindings_library}
    PRIVATE
        ${SHIBOKEN_PYTHON_INCLUDE_DIRS}
        ${Shiboken2_INCLUDE_DIRS}
        ${PySide2_INCLUDE_DIRS}
        $<TARGET_PROPERTY:libzynthbox,SOURCE_DIR>
        $<TARGET_PROPERTY:libzynthbox,INCLUDE_DIRECTORIES>
)

target_link_libraries(${bindings_library}
    PRIVATE
        Shiboken2::libshiboken
        PySide2::pyside2
        libzynthbox
)

set_property(TARGET ${bindings_library} PROPERTY PREFIX "")
set_property(TARGET ${bindings_library} PROPERTY OUTPUT_NAME "${bindings_library}")

install(
    TARGETS ${bindings_library}
    LIBRARY DESTINATION lib/python3/dist-packages/
)
