
enable_testing()

set (TESTS_PUBLIC_ARCHIVE tests_public_archive)
file (GLOB TESTS_PUBLIC_ARCHIVE_SOURCES *.cc)
list (APPEND TESTS_PUBLIC_ARCHIVE_SOURCES "archive_test_helper.cc" "../../gtest.cc"
                                           "../../test_helper.cc")

add_executable (${TESTS_PUBLIC_ARCHIVE} ${TESTS_PUBLIC_ARCHIVE_SOURCES})

include_directories (${LIBDISIR_TEST_INCLUDE_DIRS})

target_include_directories (${TESTS_PUBLIC_ARCHIVE} PRIVATE
	${CMAKE_SOURCE_DIR}/3rdparty)

target_link_libraries (${TESTS_PUBLIC_ARCHIVE} ${PROJECT_SO_LIBRARY})
target_link_libraries (${TESTS_PUBLIC_ARCHIVE} ${GTEST_BOTH_LIBRARIES})
# TODO: Why pthread not part of gtest?? (it is on fedora)
target_link_libraries (${TESTS_PUBLIC_ARCHIVE} pthread)
target_link_libraries (${TESTS_PUBLIC_ARCHIVE} stdc++fs)

add_test (LibDisirPublicAPITests ${TESTS_PUBLIC_ARCHIVE})

