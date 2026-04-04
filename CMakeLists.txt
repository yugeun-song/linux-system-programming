cmake_minimum_required(VERSION 3.10)
project(LinuxSystemProgramming C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(
    -Wall
    -Wextra
    -O0
    -ggdb3
    -fno-omit-frame-pointer
    -fno-optimize-sibling-calls
    -fasynchronous-unwind-tables
    -I${CMAKE_SOURCE_DIR}
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

file(GLOB LIB_SRCS "helper/*.c")
if(LIB_SRCS)
    add_library(helper_objs OBJECT ${LIB_SRCS})
endif()

set(SRC_DIRS user process thread memory io network ipc signal time error)

foreach(DIR ${SRC_DIRS})
    file(GLOB TARGET_SRCS "${DIR}/*.c")
    foreach(SRC_FILE ${TARGET_SRCS})
        get_filename_component(TARGET_NAME ${SRC_FILE} NAME_WE)

        set(TARGET_ID "${DIR}_${TARGET_NAME}")
        add_executable(${TARGET_ID} ${SRC_FILE})

        set_target_properties(${TARGET_ID} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${DIR}"
            OUTPUT_NAME "${TARGET_NAME}"
        )

        target_link_libraries(${TARGET_ID} PRIVATE Threads::Threads)

        if(LIB_SRCS)
            target_link_libraries(${TARGET_ID} PRIVATE $<TARGET_OBJECTS:helper_objs>)
        endif()
    endforeach()
endforeach()