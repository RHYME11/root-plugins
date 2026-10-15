add_executable(photopeak_tests PhotoPeakRootTests.cxx)
target_link_libraries(photopeak_tests PRIVATE PhotoPeakFit)
add_test(NAME photopeak_root COMMAND photopeak_tests)
add_test(NAME photopeak_loader COMMAND "${ROOT_BINDIR}/root" -l -b -q
  "${CMAKE_BINARY_DIR}/LoadPhotoPeakFit.C" "${CMAKE_CURRENT_SOURCE_DIR}/LoadSmoke.C")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../hist_fragment62276.root")
  add_test(NAME photopeak_real_spectra COMMAND photopeak_tests --data
    "${CMAKE_CURRENT_SOURCE_DIR}/../hist_fragment62276.root")
endif()
