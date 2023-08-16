project(httpmockserver-tests)

find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

set(SOURCES
    httpmockserver_tests.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::GTest
    GTest::Main
    ${PC_LIBCURL_LDFLAGS}
    httpmockserver
)

add_test(${PROJECT_NAME} ${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME} DESTINATION .)

# We create separate applications here to test the "global" TestEnvironment
set(TEST_ENV_AUTO_PROJECT "testenvironment-autoport-tests")
add_executable(${TEST_ENV_AUTO_PROJECT} testenvironment.cpp testenvironment_autoport_tests.cpp)
target_link_libraries(${TEST_ENV_AUTO_PROJECT} PRIVATE
    GTest::GTest
    GTest::Main
    ${PC_LIBCURL_LDFLAGS}
    httpmockserver
)
add_test(${TEST_ENV_AUTO_PROJECT} ${TEST_ENV_AUTO_PROJECT})
install(TARGETS ${TEST_ENV_AUTO_PROJECT} DESTINATION .)

set(TEST_ENV_FIXED_PROJECT "testenvironment-fixedport-tests")
add_executable(${TEST_ENV_FIXED_PROJECT} testenvironment.cpp testenvironment_fixedport_tests.cpp)
target_link_libraries(${TEST_ENV_FIXED_PROJECT} PRIVATE
    GTest::GTest
    GTest::Main
    ${PC_LIBCURL_LDFLAGS}
    httpmockserver
)
add_test(${TEST_ENV_FIXED_PROJECT} ${TEST_ENV_FIXED_PROJECT})
install(TARGETS ${TEST_ENV_FIXED_PROJECT} DESTINATION .)
