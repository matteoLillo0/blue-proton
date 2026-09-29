cmake_minimum_required(VERSION 3.16)
project(blue_proton_edge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # C++17 standard, senza estensioni GNU
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Tipo di build" FORCE)
endif()

# OpenCV (4.x o 5.x): inferenza YOLO (dnn), resize (imgproc), file video (videoio).
# Debian / UNO Q: sudo apt install libopencv-dev   -   Arch: sudo pacman -S opencv
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio dnn)
message(STATUS "OpenCV ${OpenCV_VERSION}")

# Parti senza dipendenze (testabili da sole).
add_library(bp_core STATIC
  src/geometry.cpp
  src/iou_tracker.cpp
  src/yolo_postprocess.cpp
  src/json_status_writer.cpp
  src/v4l2_camera_source.cpp   # solo Linux (V4L2)
)
target_include_directories(bp_core PUBLIC include)
target_compile_options(bp_core PRIVATE -Wall -Wextra -Wpedantic)

# Parti che usano OpenCV.
add_library(bp_opencv STATIC
  src/opencv_video_source.cpp
  src/opencv_yolo_detector.cpp
  src/annotated_video_writer.cpp
)
target_link_libraries(bp_opencv PUBLIC bp_core PRIVATE ${OpenCV_LIBS})
target_include_directories(bp_opencv SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
target_compile_options(bp_opencv PRIVATE -Wall -Wextra -Wpedantic)

add_executable(blue_proton_edge src/main.cpp)
target_link_libraries(blue_proton_edge PRIVATE bp_opencv)
target_compile_options(blue_proton_edge PRIVATE -Wall -Wextra -Wpedantic)

# Test: ctest --test-dir edge/build --output-on-failure
enable_testing()
add_executable(bp_tests
  tests/test_main.cpp
  tests/test_tracker.cpp
  tests/test_yolo_postprocess.cpp
  tests/fake_detector.cpp      # detector simulato: esiste SOLO nei test
)
target_link_libraries(bp_tests PRIVATE bp_core)
target_include_directories(bp_tests PRIVATE tests)
target_compile_options(bp_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME unit COMMAND bp_tests)

# Test col modello vero: gira solo se il modello e i video sono stati scaricati
# (tools/export_model.sh, tools/get_test_videos.sh).
set(BP_MODEL "${CMAKE_SOURCE_DIR}/../models/yolo11n_640.onnx")
set(BP_TEST_VIDEO "${CMAKE_SOURCE_DIR}/../data/videos/jersey.webm")
if(EXISTS "${BP_MODEL}" AND EXISTS "${BP_TEST_VIDEO}")
  add_test(NAME real_video
    COMMAND blue_proton_edge --video "${BP_TEST_VIDEO}" --no-pace --model "${BP_MODEL}" --status-file real_video_status.json)
  # Deve trovare almeno una mucca: se il detector o la riconversione delle box si rompono, fallisce.
  set_tests_properties(real_video PROPERTIES FAIL_REGULAR_EXPRESSION "animali unici 0\n")
endif()
