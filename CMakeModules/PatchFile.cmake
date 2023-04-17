# use GNU Patch from any platform
if(WIN32)
  # prioritize Git Patch on Windows as other Patches may be very old and incompatible.
  find_package(Git)
  if(Git_FOUND)
    get_filename_component(GIT_DIR ${GIT_EXECUTABLE} DIRECTORY)
    get_filename_component(GIT_DIR ${GIT_DIR} DIRECTORY)
  endif()
endif()

find_program(PATCH NAMES patch HINTS ${GIT_DIR} PATH_SUFFIXES usr/bin)

if(NOT PATCH)
  message(FATAL_ERROR "Did not find GNU Patch")
endif()

execute_process(COMMAND ${PATCH} ${in_file} --input=${patch_file} --output=${out_file} --ignore-whitespace
  TIMEOUT 15
  COMMAND_ECHO STDOUT
  RESULT_VARIABLE ret
)
if(NOT ret EQUAL 0)
  message(FATAL_ERROR "Failed to apply patch ${patch_file} to ${in_file} with ${PATCH}")
endif()

if(replace_infile)
  get_filename_component(DEST_DIR ${in_file} DIRECTORY)
  get_filename_component(DEST_NAME ${in_file} NAME)
  message(STATUS "copying ${DEST_DIR}/${DEST_NAME} to ${DEST_DIR}/${DEST_NAME}.original")
  configure_file(${DEST_DIR}/${DEST_NAME} ${DEST_DIR}/${DEST_NAME}.original COPYONLY)
  message(STATUS "copying ${out_file} to ${DEST_DIR}/${DEST_NAME}")
  configure_file(${out_file} ${DEST_DIR}/${DEST_NAME} COPYONLY)
endif()