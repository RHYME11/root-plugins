cmake_minimum_required(VERSION 3.13)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(PhotoPeakFitPlugin
    VERSION 0.1.0
    LANGUAGES CXX
  )
  option(BUILD_TESTING "Build PhotoPeakFit regression tests" OFF)
  include(CTest)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

find_package(ROOT REQUIRED COMPONENTS Core Hist Graf Gpad Matrix MathCore)
find_package(GrootPlugin REQUIRED CONFIG)
include(GNUInstallDirs)

add_library(PhotoPeakCommon OBJECT
  common/PhotoPeakFitter.cxx
)
target_include_directories(PhotoPeakCommon PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(PhotoPeakCommon PUBLIC
  ROOT::Core
  ROOT::Hist
  ROOT::Graf
  ROOT::Gpad
  ROOT::Matrix
  ROOT::MathCore
)
set_target_properties(PhotoPeakCommon PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(PhotoPeakGrootConnector OBJECT
  connectors/groot/PhotoPeakGrootPlugin.cxx
)
target_include_directories(PhotoPeakGrootConnector PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(PhotoPeakGrootConnector PRIVATE
  Groot::Plugin
  ROOT::Core
  ROOT::Hist
  ROOT::Gpad
)
set_target_properties(PhotoPeakGrootConnector PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(PhotoPeakFitPlugin SHARED
  $<TARGET_OBJECTS:PhotoPeakCommon>
  $<TARGET_OBJECTS:PhotoPeakGrootConnector>
)
target_link_libraries(PhotoPeakFitPlugin PRIVATE
  Groot::Plugin
  ROOT::Core
  ROOT::Hist
  ROOT::Graf
  ROOT::Gpad
  ROOT::Matrix
  ROOT::MathCore
)
set_target_properties(PhotoPeakFitPlugin PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

file(GENERATE
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/PhotoPeakFit.plugin
  CONTENT
"Plugin.Id: photopeakfit
Plugin.Name: PhotoPeakFit
Plugin.Version: ${PROJECT_VERSION}
Plugin.ApiVersion: 1
Plugin.Library: $<TARGET_FILE_NAME:PhotoPeakFitPlugin>
Plugin.Action.Count: 1
Plugin.Action.0.Id: photopeakfit.fit
Plugin.Action.0.Label: Peak fit
Plugin.Action.0.Tooltip: Run the minimal single-photopeak closure fit
"
)

install(TARGETS PhotoPeakFitPlugin
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/groot/plugins
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PhotoPeakFit.plugin
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/groot/plugins
)

set(_default_photopeak_test_data
  ${CMAKE_CURRENT_SOURCE_DIR}/../../macros_root/158_12_05_25_back_subtracted.root
)
set(PHOTOPEAK_TEST_DATA ${_default_photopeak_test_data} CACHE FILEPATH
  "ROOT file used for PhotoPeakFit integration tests"
)

if(BUILD_TESTING)
  if(EXISTS ${PHOTOPEAK_TEST_DATA})
    add_subdirectory(tests)
  else()
    message(WARNING
      "PhotoPeakFit tests disabled: PHOTOPEAK_TEST_DATA does not exist: "
      "${PHOTOPEAK_TEST_DATA}"
    )
  endif()
endif()
