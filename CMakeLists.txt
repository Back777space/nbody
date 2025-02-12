cmake_minimum_required(VERSION 3.16)
project(NBodySim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/include/glad/glad.c  # GLAD source file
)

include_directories(${CMAKE_SOURCE_DIR}/include)

# find and link OpenGL & GLFW
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(nbody ${SOURCES})

target_link_libraries(nbody 
    OpenGL::GL 
    glfw
    dl
)
