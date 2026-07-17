cmake_minimum_required(VERSION 3.13)

project(root_plugins
  VERSION 0.1.0
  DESCRIPTION "Optional plugins for ROOT-based applications"
  LANGUAGES CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(PhotoPeakFit)
