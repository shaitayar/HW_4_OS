project(os-hw3-tests)

Include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.0.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

add_executable(malloc_1_test malloc_1_test.cpp ${SOURCE_DIR}/malloc_1.cpp)
target_link_libraries(malloc_1_test PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(malloc_1_test TEST_PREFIX malloc_1.)

target_compile_options(malloc_1_test PRIVATE -Wall PRIVATE -pedantic-errors PRIVATE -Werror)

add_executable(malloc_2_test malloc_2_test.cpp ${SOURCE_DIR}/malloc_2.cpp)
target_link_libraries(malloc_2_test PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(malloc_2_test TEST_PREFIX malloc_2.)

target_compile_options(malloc_2_test PRIVATE -Wall PRIVATE -pedantic-errors PRIVATE -Werror)



