
add_library( ASService

        OBJECT
        asservice.cpp
        asrequest.cpp
        asvariantmap.cpp
        asqueue.cpp

        )

target_include_directories( ASService

        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}

        PRIVATE

        )

