cmake_minimum_required(VERSION 3.13)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(PhotoPeakFit VERSION 0.2.0 LANGUAGES CXX)
endif()

find_package(ROOT REQUIRED COMPONENTS Core RIO Hist Graf Gpad Gui Matrix MathCore Spectrum)
include(GNUInstallDirs)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

add_library(PhotoPeakFit SHARED
  common/PhotoPeakConfig.cxx
  common/PhotoPeakBackground.cxx
  common/PhotoPeakFitResult.cxx
  common/PhotoPeakFitter.cxx
  session/PhotoPeakSession.cxx
  session/PhotoPeakTarget.cxx
  session/PhotoPeakMarkerController.cxx
  session/PhotoPeakDrawController.cxx
  gui/PhotoPeakControlWindow.cxx
  gui/PhotoPeakParameterRow.cxx
  gui/PhotoPeakResultView.cxx
  root/PhotoPeakRoot.cxx
)
target_compile_features(PhotoPeakFit PUBLIC cxx_std_17)
target_include_directories(PhotoPeakFit PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE common gui session
)
target_link_libraries(PhotoPeakFit PUBLIC
  ROOT::Core ROOT::RIO ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::Gui
  ROOT::Matrix ROOT::MathCore ROOT::Spectrum
)
set_target_properties(PhotoPeakFit PROPERTIES
  CXX_EXTENSIONS OFF
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)

ROOT_GENERATE_DICTIONARY(G__PhotoPeakFit
  PhotoPeakFit/detail/PhotoPeakRootController.h
  MODULE PhotoPeakFit
  LINKDEF root/PhotoPeakLinkDef.h
  NOINSTALL
)

file(COPY include/PhotoPeakFit DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")
add_custom_command(TARGET PhotoPeakFit POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/include/PhotoPeakFit"
    "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}/PhotoPeakFit"
  COMMENT "Synchronizing PhotoPeakFit interpreter headers"
)
configure_file(root/LoadPhotoPeakFit.C.in
  "${CMAKE_BINARY_DIR}/LoadPhotoPeakFit.C" @ONLY)
install(TARGETS PhotoPeakFit LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/PhotoPeakFit DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_BINARY_DIR}/LoadPhotoPeakFit.C" DESTINATION .)
install(FILES
  "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libPhotoPeakFit_rdict.pcm"
  "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libPhotoPeakFit.rootmap"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

option(PHOTOPEAK_BUILD_TESTS "Build PhotoPeakFit ROOT integration tests" OFF)
if(PHOTOPEAK_BUILD_TESTS)
  enable_testing()
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../tests/photopeak"
    "${CMAKE_BINARY_DIR}/photopeak-tests")
endif()
