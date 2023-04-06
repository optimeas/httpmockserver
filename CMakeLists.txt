project(httpmockserver)

set(HEADERS
	include/httpmockserver/httpmockserver.hpp
	include/httpmockserver/testenvironment.hpp
)

set(SOURCES
	httpmockserver.cpp
	testenvironment.cpp
)

# sudo apt-get install libmicrohttpd-dev
find_package(PkgConfig REQUIRED)
pkg_search_module(MHD REQUIRED libmicrohttpd)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME}
	${MHD_LDFLAGS}
        cpp-utils
)

target_include_directories(${PROJECT_NAME}
    PUBLIC include
    PRIVATE .                 # "dot" is redundant, because local headers are always available in C/C++.
)

# We intentionally don't make the unit tests dependent on CMAKE_TESTING_ENABLED: so everyone can decide for themselves which unit tests to build
option(ENABLE_HTTPMOCKSERVER_TESTING "unit tests for httpmockserver" FALSE)
if(ENABLE_HTTPMOCKSERVER_TESTING)
    add_subdirectory(tests)
endif()
