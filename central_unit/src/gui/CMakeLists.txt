# Qt config
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Cross compile not detecting WrapOpenGL fix
if (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES
            INTERFACE_LINK_LIBRARIES "/usr/lib/aarch64-linux-gnu/libEGL.so;/usr/lib/aarch64-linux-gnu/libGLESv2.so"
    )
endif ()

find_package(Qt6 REQUIRED COMPONENTS
        Core
        Quick
        Qml
)

set(SOURCES
        main.cpp
        api_client.cpp
)

set(QML_FILES
        qml/Main.qml

        qml/pages/SettingsPage.qml
        qml/pages/StatusPage.qml
        qml/pages/HomePage.qml
        qml/pages/ModuleDetailPage.qml

        qml/components/CustomTabButton.qml
        qml/components/SettingsButton.qml
        qml/components/DialogButton.qml
)

qt_add_executable(smarthomegui ${SOURCES})

qt_add_qml_module(smarthomegui
        URI Smarthome
        VERSION 0.1
        QML_FILES ${QML_FILES}
)

target_link_libraries(smarthomegui PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::Qml
        smarthome::logger
        smarthome::utility
)