cmake_minimum_required(VERSION 3.13)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(MatrixGate VERSION 0.3.0 LANGUAGES CXX)
endif()

find_package(ROOT REQUIRED COMPONENTS Core RIO Hist Matrix MathCore Spectrum Minuit2 Graf Gpad Gui)
include(GNUInstallDirs)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

add_library(MatrixGate SHARED
  common/MatrixGateConfig.cxx
  common/MatrixGateBackground.cxx
  common/MatrixGateProjection.cxx
  common/MatrixGateResult.cxx
  session/MatrixGateSession.cxx
  gui/MatrixGateWindow.cxx
  gui/MatrixGateReportView.cxx
  root/MatrixGateRoot.cxx
  root/MatrixGateRootController.cxx
)
target_compile_features(MatrixGate PUBLIC cxx_std_17)
target_include_directories(MatrixGate PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE common session gui
)
target_link_libraries(MatrixGate PUBLIC ROOT::Core ROOT::RIO ROOT::Hist
  ROOT::Matrix ROOT::MathCore ROOT::Spectrum ROOT::Minuit2 ROOT::Graf ROOT::Gpad ROOT::Gui)
set_target_properties(MatrixGate PROPERTIES
  CXX_EXTENSIONS OFF
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
)

ROOT_GENERATE_DICTIONARY(G__MatrixGate
  MatrixGate/MatrixGateRoot.h
  MatrixGate/detail/MatrixGateRootController.h
  MODULE MatrixGate
  LINKDEF root/MatrixGateLinkDef.h
  NOINSTALL
)

file(COPY include/MatrixGate DESTINATION "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")
add_custom_command(TARGET MatrixGate POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/include/MatrixGate"
    "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}/MatrixGate"
  COMMENT "Synchronizing MatrixGate interpreter headers"
)
configure_file(root/LoadMatrixGate.C.in "${CMAKE_BINARY_DIR}/LoadMatrixGate.C" @ONLY)
install(TARGETS MatrixGate LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/MatrixGate DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_BINARY_DIR}/LoadMatrixGate.C" DESTINATION .)
install(FILES
  "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libMatrixGate_rdict.pcm"
  "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libMatrixGate.rootmap"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

option(MATRIXGATE_BUILD_TESTS "Build local MatrixGate projection and loader tests" OFF)
if(MATRIXGATE_BUILD_TESTS)
  enable_testing()
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../tests/matrixgate/CMakeLists.txt")
    message(FATAL_ERROR "MATRIXGATE_BUILD_TESTS requires local tests/matrixgate sources.")
  endif()
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../tests/matrixgate"
    "${CMAKE_BINARY_DIR}/matrixgate-tests")
endif()
