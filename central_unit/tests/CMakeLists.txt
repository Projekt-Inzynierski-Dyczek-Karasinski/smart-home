include(FetchContent)
FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW ON
        SYSTEM
        EXCLUDE_FROM_ALL)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)
set_target_properties(gtest gtest_main PROPERTIES FOLDER "third_party")

include(GoogleTest)

add_subdirectory(core)
