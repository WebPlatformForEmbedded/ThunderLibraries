# - Try to find the Breakpad library.
#
# The following are set after configuration is done:
#  BREAKPAD_FOUND
#  BREAKPAD_INCLUDE_DIRS
#  BREAKPAD_LIBRARY_DIRS
#  BREAKPAD_LIBRARIES

find_path( BREAKPAD_INCLUDE_DIR NAMES breakpad )
find_library( BREAKPAD_LIBRARY NAMES libbreakpad_client.a )

message( "BREAKPAD_INCLUDE_DIR include dir = ${BREAKPAD_INCLUDE_DIR}" )
message( "BREAKPAD_LIBRARY lib = ${BREAKPAD_LIBRARY}" )

set( BREAKPAD_LIBRARIES ${BREAKPAD_LIBRARY} )
set( BREAKPAD_INCLUDE_DIRS ${BREAKPAD_INCLUDE_DIR}/breakpad/ )

include(FindPackageHandleStandardArgs)

# Handle the QUIETLY and REQUIRED arguments and set the LM_SENSORS_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(BREAKPAD DEFAULT_MSG
        BREAKPAD_LIBRARY BREAKPAD_INCLUDE_DIR)

mark_as_advanced(BREAKPAD_INCLUDE_DIR BREAKPAD_LIBRARY)
