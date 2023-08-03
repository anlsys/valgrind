# CMake Minimum version
cmake_minimum_required(VERSION 3.10)

project(Taskgrind_OMP_Tests LANGUAGES CXX)

add_executable(Taskgrind_OMP_Tests src/Taskgrind_OMP_Tests.c)
target_link_libraries(Taskgrind_OMP_Tests taskgrind_omp)
add_test(NAME Taskgrind_OMP_Tests COMMAND Taskgrind_OMP_Tests)
