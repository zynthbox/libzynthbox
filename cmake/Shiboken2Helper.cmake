find_package(Shiboken2)

find_path(Shiboken2_INCLUDE_BASEDIR include/shiboken2)
set(Shiboken2_INCLUDE_DIRS
        ${Shiboken2_INCLUDE_BASEDIR}/include/shiboken2
        ${Qt5Core_INCLUDE_DIRS}
        ${Qt5Gui_INCLUDE_DIRS}
        ${Qt5Network_INCLUDE_DIRS}
        ${Qt5Qml_INCLUDE_DIRS}
)
