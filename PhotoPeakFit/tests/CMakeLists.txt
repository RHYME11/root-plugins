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
  $<TARGET_OBJECTS:PhotoPeakSession>
  $<TARGET_OBJECTS:PhotoPeakGrootConnector>
)
target_include_directories(PhotoPeakConnectorTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../connectors/groot
)
target_compile_definitions(PhotoPeakConnectorTest PRIVATE
  PHOTOPEAK_TEST_DATA="${PHOTOPEAK_TEST_DATA}"
)
target_link_libraries(PhotoPeakConnectorTest PRIVATE
  GrootPluginApi
  GrootRuntime
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::Gui ROOT::Spectrum
  ROOT::Matrix ROOT::MathCore
)
add_test(NAME PhotoPeakConnector COMMAND PhotoPeakConnectorTest)

add_executable(PhotoPeakGrootInteractionTest
  PhotoPeakGrootInteractionTest.cxx
  $<TARGET_OBJECTS:PhotoPeakCommon>
  $<TARGET_OBJECTS:PhotoPeakSession>
  $<TARGET_OBJECTS:PhotoPeakGrootConnector>
)
target_include_directories(PhotoPeakGrootInteractionTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../connectors/groot
)
target_link_libraries(PhotoPeakGrootInteractionTest PRIVATE
  GrootPluginApi
  GrootRuntime
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::Gui ROOT::Spectrum
  ROOT::Matrix ROOT::MathCore
)
add_test(NAME PhotoPeakGrootInteraction COMMAND PhotoPeakGrootInteractionTest)

add_executable(PhotoPeakSessionTest
  PhotoPeakSessionTest.cxx
  $<TARGET_OBJECTS:PhotoPeakCommon>
  $<TARGET_OBJECTS:PhotoPeakSession>
)
target_include_directories(PhotoPeakSessionTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(PhotoPeakSessionTest PRIVATE
  ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::Gui ROOT::Spectrum
  ROOT::Matrix ROOT::MathCore
)
add_test(NAME PhotoPeakSession COMMAND PhotoPeakSessionTest)

add_executable(PhotoPeakFullLoopTest
  PhotoPeakFullLoopTest.cxx
)
target_compile_definitions(PhotoPeakFullLoopTest PRIVATE
  PHOTOPEAK_TEST_DATA="${PHOTOPEAK_TEST_DATA}"
  PHOTOPEAK_PLUGIN_DIRECTORY="$<TARGET_FILE_DIR:PhotoPeakFitPlugin>"
  PHOTOPEAK_PLUGIN_LIBRARY="$<TARGET_FILE_NAME:PhotoPeakFitPlugin>"
)
target_link_libraries(PhotoPeakFullLoopTest PRIVATE
  GrootPluginRuntime
  ROOT::Core ROOT::Hist ROOT::Gpad
)
add_dependencies(PhotoPeakFullLoopTest PhotoPeakFitPlugin)
add_test(NAME PhotoPeakFullLoop COMMAND PhotoPeakFullLoopTest)
