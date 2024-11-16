# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\yolov8-pose_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\yolov8-pose_autogen.dir\\ParseCache.txt"
  "yolov8-pose_autogen"
  )
endif()
