find_package(PkgConfig REQUIRED)
pkg_check_modules(ImGui REQUIRED imgui>=1.90.1)
pkg_check_modules(Sdl3 REQUIRED sdl3>=3.2.10)


add_executable(smarthomegui
        main.cpp
        gui.cpp
        gui.h
)

target_include_directories(smarthomegui PUBLIC ${imgui_SOURCE_DIR})

target_link_libraries(smarthomegui PRIVATE
        smarthome::ipc
        smarthome::utility
)