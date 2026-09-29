cmake_minimum_required(VERSION 3.16)
project(blue_proton_edge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)   # C++17 standard, senza estensioni GNU
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Tipo di build" FORCE)
endif()

# Libreria con interfacce e implementazioni: chi scrive un pezzo vero
# (detector, tracker, sorgente camera) aggiunge qui il suo .cpp.
add_library(bp_edge STATIC
  src/fake_frame_source.cpp
  src/fake_detector.cpp
  src/null_tracker.cpp
  src/iou_tracker.cpp
  src/json_status_writer.cpp
  src/v4l2_camera_source.cpp   # solo Linux (V4L2)
)
target_include_directories(bp_edge PUBLIC include)
target_compile_options(bp_edge PRIVATE -Wall -Wextra -Wpedantic)

# OpenCV e' OPZIONALE: se c'e' si compila anche la sorgente da file video (--video),
# altrimenti tutto il resto funziona uguale. Per disattivarla: -DBP_WITH_OPENCV=OFF
option(BP_WITH_OPENCV "Sorgente da file video con OpenCV" ON)
if(BP_WITH_OPENCV)
  find_package(OpenCV QUIET COMPONENTS core imgproc videoio)
endif()
if(OpenCV_FOUND)
  message(STATUS "OpenCV ${OpenCV_VERSION}: sorgente video attiva")
  target_sources(bp_edge PRIVATE src/opencv_video_source.cpp)
  target_link_libraries(bp_edge PRIVATE ${OpenCV_LIBS})
  target_include_directories(bp_edge SYSTEM PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_compile_definitions(bp_edge PUBLIC BP_HAVE_OPENCV)
else()
  message(STATUS "OpenCV non trovato: niente sorgente video (--video)")
endif()

add_executable(blue_proton_edge src/main.cpp)
target_link_libraries(blue_proton_edge PRIVATE bp_edge)
target_compile_options(blue_proton_edge PRIVATE -Wall -Wextra -Wpedantic)

# Test: ctest --test-dir edge/build --output-on-failure
enable_testing()
add_executable(test_tracker tests/test_tracker.cpp)
target_link_libraries(test_tracker PRIVATE bp_edge)
target_compile_options(test_tracker PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME tracker COMMAND test_tracker)
