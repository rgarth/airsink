cmake_minimum_required(VERSION 3.10)
project(airsink VERSION 0.1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)
pkg_check_modules(ALSA REQUIRED alsa)
pkg_check_modules(AVAHI REQUIRED avahi-client avahi-core)
pkg_check_modules(JSONC REQUIRED json-c)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${OPENSSL_INCLUDE_DIR}
    ${LIBEVENT_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${AVAHI_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
)

# Add source files
file(GLOB_RECURSE SOURCES 
    "src/*.c"
)

# Create executable
add_executable(airsink ${SOURCES})

# Link libraries
target_link_libraries(airsink
    ${OPENSSL_LIBRARIES}
    ${LIBEVENT_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${AVAHI_LIBRARIES}
    ${JSONC_LIBRARIES}
)

# Install
install(TARGETS airsink DESTINATION bin) 