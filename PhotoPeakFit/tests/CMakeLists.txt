add_executable(PhotoPeakCommonTest
  PhotoPeakCommonTest.cxx
  $<TARGET_OBJECTS:PhotoPeakCommon>
)
target_include_directories(PhotoPeakCommonTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_compile_definitions(PhotoPeakCommonTest PRIVATE
  PHOTOPEAK_TEST_DATA="${PHOTOPEAK_TEST_DATA}"
)
target_link_libraries(PhotoPeakCommonTest PRIVATE
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::Matrix ROOT::MathCore
)
add_test(NAME PhotoPeakCommon COMMAND PhotoPeakCommonTest)

add_executable(PhotoPeakConnectorTest
  PhotoPeakConnectorTest.cxx
  $<TARGET_OBJECTS:PhotoPeakCommon>
  $<TARGET_OBJECTS:PhotoPeakGrootConnector>
)
target_include_directories(PhotoPeakConnectorTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_compile_definitions(PhotoPeakConnectorTest PRIVATE
  PHOTOPEAK_TEST_DATA="${PHOTOPEAK_TEST_DATA}"
)
target_link_libraries(PhotoPeakConnectorTest PRIVATE
  Groot::Plugin
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::Matrix ROOT::MathCore
)
add_test(NAME PhotoPeakConnector COMMAND PhotoPeakConnectorTest)

add_executable(PhotoPeakFullLoopTest
  PhotoPeakFullLoopTest.cxx
)
target_compile_definitions(PhotoPeakFullLoopTest PRIVATE
  PHOTOPEAK_TEST_DATA="${PHOTOPEAK_TEST_DATA}"
  PHOTOPEAK_PLUGIN_DIRECTORY="$<TARGET_FILE_DIR:PhotoPeakFitPlugin>"
  PHOTOPEAK_PLUGIN_LIBRARY="$<TARGET_FILE_NAME:PhotoPeakFitPlugin>"
)
target_link_libraries(PhotoPeakFullLoopTest PRIVATE
  Groot::Plugin
  ROOT::Core ROOT::Hist ROOT::Gpad
)
add_dependencies(PhotoPeakFullLoopTest PhotoPeakFitPlugin)
add_test(NAME PhotoPeakFullLoop COMMAND PhotoPeakFullLoopTest)
