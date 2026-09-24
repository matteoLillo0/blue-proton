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
  src/json_status_writer.cpp
)
target_include_directories(bp_edge PUBLIC include)
target_compile_options(bp_edge PRIVATE -Wall -Wextra -Wpedantic)

add_executable(blue_proton_edge src/main.cpp)
target_link_libraries(blue_proton_edge PRIVATE bp_edge)
target_compile_options(blue_proton_edge PRIVATE -Wall -Wextra -Wpedantic)
