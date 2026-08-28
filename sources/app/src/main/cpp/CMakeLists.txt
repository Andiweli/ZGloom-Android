cmake_minimum_required(VERSION 3.10.2)

project(ZGloomAndroid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect the common ZGloom C++ core.
#
# The Android project deliberately still supports CMake 3.10.2.  That version
# cannot reliably notice newly added files in a GLOB from an already configured
# Gradle/CMake build directory.  Keep the recursive collection for the existing
# portable core, but append newly introduced standalone modules explicitly so a
# changed CMakeLists.txt forces Gradle to regenerate the native build model.
file(GLOB_RECURSE ZGLOOM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ZGloom/*.cpp"
)

set(ZGLOOM_EXPLICIT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ZGloom/DefenderGame.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ZGloom/RetroTouchBridge.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ZGloom/RetroTouchInput.cpp"
)

foreach(ZGLOOM_SOURCE IN LISTS ZGLOOM_EXPLICIT_SOURCES)
    if(NOT EXISTS "${ZGLOOM_SOURCE}")
        message(FATAL_ERROR "Required ZGloom source is missing: ${ZGLOOM_SOURCE}")
    endif()

    # Avoid a duplicate when configuring the project from a completely clean
    # directory where the recursive GLOB already found the file.
    list(REMOVE_ITEM ZGLOOM_SOURCES "${ZGLOOM_SOURCE}")
    list(APPEND ZGLOOM_SOURCES "${ZGLOOM_SOURCE}")
endforeach()

list(SORT ZGLOOM_SOURCES)

if(ZGLOOM_SOURCES STREQUAL "")
    message(FATAL_ERROR "No ZGloom .cpp sources found under src/main/cpp/ZGloom. Please ensure the PC sources are copied correctly.")
endif()

# Build as SHARED library 'main' so that the Java/SDL side can load libmain.so.
add_library(main SHARED ${ZGLOOM_SOURCES})

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ZGloom
    ${CMAKE_CURRENT_SOURCE_DIR}/ZGloom/xmp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2
)

# Import prebuilt SDL2 / SDL2_mixer / xmp shared libraries from a custom prebuilt folder.
# This avoids conflicts with jniLibs automatic packaging.
#
# Expected layout in your Android project:
#   app/src/main/prebuilt/armeabi-v7a/libSDL2.so
#   app/src/main/prebuilt/armeabi-v7a/libSDL2_mixer.so
#   app/src/main/prebuilt/armeabi-v7a/libxmp.so

add_library(SDL2 SHARED IMPORTED)
set_target_properties(SDL2 PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../prebuilt/${ANDROID_ABI}/libSDL2.so"
)

add_library(SDL2_mixer SHARED IMPORTED)
set_target_properties(SDL2_mixer PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../prebuilt/${ANDROID_ABI}/libSDL2_mixer.so"
)

add_library(xmp SHARED IMPORTED)
set_target_properties(xmp PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../prebuilt/${ANDROID_ABI}/libxmp.so"
)

# Android system libraries.
find_library(log-lib log)
find_library(android-lib android)

# Link against SDL2, mixer, xmp and system libraries.
target_link_libraries(main
    SDL2
    SDL2_mixer
    xmp
    ${android-lib}
    ${log-lib}
)
