
add_library( EventLoop

        OBJECT
        eventloop.cpp
        timer.cpp
        ionotifier.cpp
        signalnotifier.cpp
        childnotifier.cpp

        )

target_include_directories( EventLoop

        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}

        )

