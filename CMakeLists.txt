cmake_minimum_required(VERSION 3.14)

project(flavor_runtime LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library (flavor_runtime SHARED qbitstream.cpp smallvector.cpp)

target_include_directories (flavor_runtime 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# versioning
set(FLAVOR_VERSION_MAJOR 0)
set(FLAVOR_VERSION_MINOR 0)
set(FLAVOR_VERSION_PATCH 1)
set(FLAVOR_VERSION ${FLAVOR_VERSION_MAJOR}.${FLAVOR_VERSION_MINOR}.${FLAVOR_VERSION_PATCH})
set_target_properties(flavor_runtime    PROPERTIES 
                                        VERSION ${FLAVOR_VERSION}
                                        SOVERSION ${FLAVOR_VERSION_MAJOR})

install(TARGETS flavor_runtime
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/flavor"
        FILES_MATCHING
        PATTERN "*.h"
)