if(NOT DEFINED ENV{DISPLAY} OR "$ENV{DISPLAY}" STREQUAL "")
  message("SKIP: level tour requires DISPLAY")
  return()
endif()
execute_process(COMMAND "${GAME_EXE}" --level-tour --frames 5000 --no-audio --no-vsync
  --user-data "${USER_DATA}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 85)
if(NOT result EQUAL 0 OR NOT output MATCHES "entered=ANNEX spawn=-15,0" OR
   NOT output MATCHES "level=ANNEX level_changes=1" OR NOT output MATCHES "collected=3")
  message(FATAL_ERROR "Level tour failed: ${result}\n${output}\n${error}")
endif()
message(STATUS "Both authored levels completed through the native game loop")
