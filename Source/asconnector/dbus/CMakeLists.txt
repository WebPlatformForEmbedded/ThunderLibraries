
add_library( DBus

             OBJECT
             dbusconnection.cpp
             dbusmessage.cpp
             dbusfiledescriptor.cpp

             )

target_include_directories( DBus

             PUBLIC
             ${CMAKE_CURRENT_LIST_DIR}

             PRIVATE

             )

