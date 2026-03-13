cmake_minimum_required(VERSION 3.16)
project(SpectrogramViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt ────────────────────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Widgets Concurrent QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets Concurrent REQUIRED)
    message(STATUS "Using Qt5")
else()
    message(STATUS "Using Qt6")
endif()

# ── pkg-config ────────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)

# ── PortAudio ─────────────────────────────────────────────────────────────────
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# ── libsndfile ────────────────────────────────────────────────────────────────
pkg_check_modules(SNDFILE REQUIRED sndfile)

# ── FFTW3 (single precision – libfftw3f) ─────────────────────────────────────
pkg_check_modules(FFTW3F REQUIRED fftw3f)

# ── FFmpeg ────────────────────────────────────────────────────────────────────
pkg_check_modules(AVFORMAT   REQUIRED libavformat)
pkg_check_modules(AVCODEC    REQUIRED libavcodec)
pkg_check_modules(AVUTIL     REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

# ── OpenCV ────────────────────────────────────────────────────────────────────
# We need core + videoio + imgproc
find_package(OpenCV REQUIRED COMPONENTS core videoio imgproc)
if(OpenCV_FOUND)
    message(STATUS "OpenCV ${OpenCV_VERSION} found — includes: ${OpenCV_INCLUDE_DIRS}")
else()
    message(FATAL_ERROR "OpenCV not found. Install libopencv-dev (Ubuntu) or brew install opencv (macOS).")
endif()

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    AudioDecoder.cpp
    AudioPlayer.cpp
    SpectrogramWidget.cpp
    MainWindow.cpp
    VideoDecoder.cpp
    VideoPlayerWidget.cpp
)

set(HEADERS
    AudioDecoder.h
    AudioPlayer.h
    SpectrogramWidget.h
    MainWindow.h
    VideoDecoder.h
    VideoPlayerWidget.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# ── Include dirs ──────────────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PORTAUDIO_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${FFTW3F_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# ── Link ──────────────────────────────────────────────────────────────────────
if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Concurrent)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets Qt5::Concurrent)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PORTAUDIO_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    ${FFTW3F_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${OpenCV_LIBS}          # opencv_core opencv_videoio opencv_imgproc
)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${PORTAUDIO_CFLAGS_OTHER}
    ${SNDFILE_CFLAGS_OTHER}
    ${FFTW3F_CFLAGS_OTHER}
    ${AVFORMAT_CFLAGS_OTHER}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
