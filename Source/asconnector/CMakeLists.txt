cmake_minimum_required( VERSION 3.10.3 )

project( asconnector )

option( ENABLE_BREAKPAD_MINIDUMP "Enables the support for Google Breakpad minidump" ON )

# Add our local cmake directory to search for components
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake" )

# Thread is just the generic lib link for the pthread libraries (on platforms
# that don't have a separate pthread library this is a NOP)
find_package( Threads REQUIRED )

# Find the systemd libraries
find_package( systemd REQUIRED )
find_package( udev REQUIRED )

Find Library for Tracing and JSON
find_package(Thunder REQUIRED)
find_package(ThunderMessaging REQUIRED)

# We require C++14
set( CMAKE_CXX_STANDARD 14 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# Enable below 2 lines for code coverage
###set( CMAKE_CXX_FLAGS "-g -coverage -fprofile-arcs -ftest-coverage -mfloat-abi=hard")
###set( CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)

# The following disables an annoying warning "<blah> will change in GCC X.XX"
add_compile_options( -Wno-psabi )

include_directories(${CMAKE_CURRENT_LIST_DIR}/asservice/)
include_directories(${CMAKE_CURRENT_LIST_DIR}/System/)
include_directories(${CMAKE_CURRENT_LIST_DIR} )
include_directories(${SYSTEMD_INCLUDE_DIRS} )

add_subdirectory( eventloop )
add_subdirectory( dbus )
add_subdirectory( asservice )

if ( ENABLE_BREAKPAD_MINIDUMP )
    message( STATUS "ENABLE_BREAKPAD_MINIDUMP is on" )

    # Find the breakpad library
    find_package( breakpad REQUIRED )

    include_directories( ${BREAKPAD_INCLUDE_DIRS} )
    add_definitions( -DENABLE_BREAKPAD_MINIDUMP=1 )
endif()

add_executable(
        ASSystemService

        main.cpp

        $<TARGET_OBJECTS:EventLoop>
        $<TARGET_OBJECTS:DBus>
        $<TARGET_OBJECTS:ASService>
        $<TARGET_OBJECTS:InputHandler>
)

target_link_libraries(

        ThunderProtocols::ThunderProtocols

        ${SYSTEMD_LIBRARIES}

        # Adds pthread support (if it's a separate library on target)
        Threads::Threads

        # breakpad libraries
        $<$<BOOL:${ENABLE_BREAKPAD_MINIDUMP}>:${BREAKPAD_LIBRARIES}>

        # Add librt needed by libsystemd
        rt
        ds

        # enable gcov for code coverage
        #gcov
        UDEV::libudev
)

# Installs the ASSystemService daemon
install(
        TARGETS ASSystemService
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

# Install the Service dependencies JSON file
install(
        FILES       config/service_dependencies.json
        DESTINATION /usr/share/assystem/
)

# Install a dbus config file for the ASSystemService daemon
install(
        FILES       config/com.sky.as.system.conf
        DESTINATION /etc/dbus-1/system.d/

        )

# Install AS version file for the ASSystemService daemon
install(
        FILES       as_version.json
        DESTINATION /opt/persistent/sky/

)
