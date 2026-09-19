cmake_minimum_required(VERSION 3.10)

project(NaiveMatrixMultiplication C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


find_package(OpenMP)


set(OPENBLAS_ROOT "$ENV{OPENBLAS_ROOT}" CACHE PATH "OpenBLAS installation prefix")
if(NOT EXISTS "${OPENBLAS_ROOT}/lib/libopenblas.so" AND NOT EXISTS "${OPENBLAS_ROOT}/lib/libopenblas.a")
    message(FATAL_ERROR "OpenBLAS not found. Set OPENBLAS_ROOT to a user-local installation.")
endif()

add_executable(run_matrix_multiplication
    src/main.c
    task/task.c
    src/activ_baseline.c
    task/activation.c
    task/blas_reference.c
)
target_include_directories(run_matrix_multiplication PRIVATE "${OPENBLAS_ROOT}/include")
target_link_directories(run_matrix_multiplication PRIVATE "${OPENBLAS_ROOT}/lib")
set_target_properties(run_matrix_multiplication PROPERTIES
    BUILD_RPATH "${OPENBLAS_ROOT}/lib"
)
target_link_libraries(run_matrix_multiplication m)
if(OpenMP_C_FOUND)
    target_link_libraries(run_matrix_multiplication OpenMP::OpenMP_C)
endif()
