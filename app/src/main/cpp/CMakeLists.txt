cmake_minimum_required(VERSION 3.22.1)

project("velosphere" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GameActivity prefab package provided via AAR dependency
find_package(game-activity REQUIRED CONFIG)

# Build the native shared library for VeloSphere
add_library(native-lib SHARED
    main.cpp
)

target_include_directories(native-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link GameActivity static wrapper along with Android NDK and graphics libraries
target_link_libraries(native-lib
    game-activity::game-activity_static
    android
    log
    EGL
    GLESv3
)

# Support 16 KB memory page sizes by aligning ELF segments
target_link_options(native-lib PRIVATE
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
)

