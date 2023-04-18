# Macro to get various pyside / python include / link flags and paths.
# Uses the not entirely supported utils/pyside2_config.py file.
macro(pyside2_config option output_var)
    if(${ARGC} GREATER 2)
        set(is_list ${ARGV2})
    else()
        set(is_list "")
    endif()

    execute_process(
      COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/pyside2_config.py" ${option}
      OUTPUT_VARIABLE ${output_var}
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if ("${${output_var}}" STREQUAL "")
        message(FATAL_ERROR "Error: Calling pyside2_config.py ${option} returned no output.")
    endif()
    if(is_list)
        string (REPLACE " " ";" ${output_var} "${${output_var}}")
    endif()
endmacro()

set(bindings_library "Zynthbox")
set(wrapped_header ${CMAKE_CURRENT_SOURCE_DIR}/bindings.h)
set(typesystem_file ${CMAKE_CURRENT_SOURCE_DIR}/bindings.xml)

set(generated_sources
        ${BINDINGS_OUTPUT_DIR}/${bindings_library}/zynthbox_module_wrapper.cpp
        ${BINDINGS_OUTPUT_DIR}/${bindings_library}/plugin_wrapper.cpp
        ${BINDINGS_OUTPUT_DIR}/${bindings_library}/audiolevels_wrapper.cpp
)


### BEGIN Shiboken detection

# Set shiboken path, Python path, include paths and linker flags.
set(pyside2_path /usr/lib/python3/dist-packages/PySide2)
set(python_include_dir /usr/include/python3.9)
set(shiboken_include_dir /usr/include/shiboken2)
set(shiboken_shared_libraries /usr/lib/arm-linux-gnueabihf/libshiboken2.cpython-39-arm-linux-gnueabihf.so)
set(pyside2_shared_libraries /usr/lib/arm-linux-gnueabihf/libpyside2.cpython-39-arm-linux-gnueabihf.so)
set(python_linking_data /usr/lib;python3.9)
set(shiboken_path /usr/bin/shiboken2)
set(qt5_include_dir /usr/include/arm-linux-gnueabihf/qt5/)

if(NOT EXISTS ${shiboken_path})
    message(FATAL_ERROR "Shiboken executable not found at path: ${shiboken_path}")
endif()

### END Shiboken detection


### BEGIN Shiboken target for generating binding C++ files

# Set up the options to pass to shiboken.
# set(shiboken_options
#         --generator-set=shiboken
#         --enable-parent-ctor-heuristic
#         --enable-return-value-heuristic
#         --use-isnull-as-nb_nonzero
#         --avoid-protected-hack
#         -I${CMAKE_SOURCE_DIR}
#         -T${CMAKE_SOURCE_DIR}
#         --output-directory=${CMAKE_CURRENT_BINARY_DIR}/${bindings_library}
# )

set(generated_sources_dependencies ${wrapped_header} ${typesystem_file})

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
                --output-directory=${BINDINGS_OUTPUT_DIR}
                --typesystem-paths=/usr/share/PySide2/typesystems/
                -I${qt5_include_dir}
                -I${qt5_include_dir}/QtCore
                -I${qt5_include_dir}/QtGui
                -I${qt5_include_dir}/QtQml
                -I${qt5_include_dir}/QtNetwork
                -I${CMAKE_SOURCE_DIR}/tracktion_engine/modules/
                -I${CMAKE_SOURCE_DIR}/tracktion_engine/modules/juce/modules/
                ${wrapped_header}
                ${typesystem_file}
    DEPENDS ${generated_sources_dependencies}
    IMPLICIT_DEPENDS CXX ${wrapped_header}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running generator for ${typesystem_file}."
)

### END Shiboken target for generating binding C++ files


### BEGIN CMake target - bindings_library

# Define and build the bindings library.
add_library(${bindings_library} MODULE ${generated_sources})

target_include_directories(${bindings_library}
    PUBLIC
        /usr/include/PySide2/
        /usr/include/PySide2/QtCore
        /usr/include/PySide2/QtGui
        # /usr/include/PySide2/QtQml
        /usr/include/PySide2/QtNetwork
        /zynthian/libzynthbox/pyside_bindings
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Apply relevant include and link flags.
target_include_directories(${bindings_library} PRIVATE ${python_include_dir})
target_include_directories(${bindings_library} PRIVATE ${shiboken_include_dir})
target_include_directories(${bindings_library} PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(${bindings_library} PRIVATE ${shiboken_shared_libraries})
target_link_libraries(${bindings_library} PRIVATE zynthbox ${pyside2_shared_libraries})

set_property(TARGET ${bindings_library} PROPERTY PREFIX "")
set_property(TARGET ${bindings_library} PROPERTY OUTPUT_NAME "${bindings_library}${PYTHON_EXTENSION_SUFFIX}")

install(
    TARGETS ${bindings_library}
    LIBRARY DESTINATION lib/python3/dist-packages/
)

### END CMake target - bindings_library
