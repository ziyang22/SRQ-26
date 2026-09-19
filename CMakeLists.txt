cmake_minimum_required(VERSION 3.10)

project(NaiveMatrixMultiplication C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


find_package(OpenMP)


add_executable(run_matrix_multiplication
    src/main.c
    task/task.c
    src/activ_baseline.c
    task/activation.c
)
target_link_libraries(run_matrix_multiplication m)
if(OpenMP_C_FOUND)
    target_link_libraries(run_matrix_multiplication OpenMP::OpenMP_C)
endif()
