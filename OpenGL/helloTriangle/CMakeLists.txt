cmake_minimum_required(VERSION 2.8)

project(helloTriangle)
add_executable(helloTriangle helloTriangle.cpp)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
include_directories( ${OPENGL_INCLUDE_DIRS}  ${GLUT_INCLUDE_DIRS} )

target_link_libraries(testas ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} )
