set(bindings_library "Zynthbox")
set(wrapped_header ${CMAKE_CURRENT_SOURCE_DIR}/bindings.h)
set(typesystem_file ${CMAKE_CURRENT_SOURCE_DIR}/bindings.xml)
set(typesystems_path /usr/share/PySide2/typesystems/)
set(generated_sources
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/zynthbox_module_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/plugin_wrapper.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}/audiolevels_wrapper.cpp
)
set(python_include_dir /usr/include/python3.9)
set(shiboken_include_dir /usr/include/shiboken2)
set(pyside2_include_dir /usr/include/PySide2)
set(shiboken_shared_libraries /usr/lib/arm-linux-gnueabihf/libshiboken2.cpython-39-arm-linux-gnueabihf.so)
set(pyside2_shared_libraries /usr/lib/arm-linux-gnueabihf/libpyside2.cpython-39-arm-linux-gnueabihf.so)
set(shiboken_path /usr/bin/shiboken2)
set(qt5_include_dir /usr/include/arm-linux-gnueabihf/qt5/)
set(generated_sources_dependencies ${wrapped_header} ${typesystem_file})

if(NOT EXISTS ${shiboken_path})
    message(FATAL_ERROR "Shiboken executable not found at path: ${shiboken_path}")
endif()

# Add custom target to run shiboken to generate the binding cpp files.
add_custom_command(
    OUTPUT ${generated_sources}
    COMMAND shiboken2
                --debug-level=full
                --generator-set=shiboken
                --enable-parent-ctor-heuristic
                --enable-return-value-heuristic
                --enable-pyside-extensions
                --use-isnull-as-nb_nonzero
                --language-level=c++17
                --output-directory=${CMAKE_CURRENT_BINARY_DIR}
                --typesystem-paths=${typesystems_path}
                -I${qt5_include_dir}
                -I${qt5_include_dir}/QtCore
                -I${CMAKE_SOURCE_DIR}/tracktion_engine/modules/
                -I${CMAKE_SOURCE_DIR}/tracktion_engine/modules/juce/modules/
                ${wrapped_header}
                ${typesystem_file}
    DEPENDS ${generated_sources_dependencies}
    IMPLICIT_DEPENDS CXX ${wrapped_header}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running generator for ${typesystem_file}."
)

# Define and build the bindings library.
add_library(${bindings_library} MODULE ${generated_sources})

# Apply relevant include and link flags.
target_include_directories(${bindings_library}
    PRIVATE
        ${python_include_dir}
        ${shiboken_include_dir}
        ${pyside2_include_dir}
        ${pyside2_include_dir}/QtCore
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${bindings_library}
    PRIVATE
        ${shiboken_shared_libraries}
        ${pyside2_shared_libraries}
        zynthbox
)

set_property(TARGET ${bindings_library} PROPERTY PREFIX "")
set_property(TARGET ${bindings_library} PROPERTY OUTPUT_NAME "${bindings_library}")

install(
    TARGETS ${bindings_library}
    LIBRARY DESTINATION lib/python3/dist-packages/
)
