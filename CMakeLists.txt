cmake_minimum_required(VERSION 3.20)
project(CleanRenderer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies: GLFW + GLM via FetchContent
include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# GLAD (local)
add_library(glad STATIC
  external/glad/src/glad.c
)
target_include_directories(glad PUBLIC
  external/glad/include
)

# App
add_executable(CleanRenderer
  src/main.cpp
  src/Shader.h
  src/Shader.cpp
  src/OrbitCamera.h
)

target_link_libraries(CleanRenderer PRIVATE
  glfw
  glad
  glm::glm
)

# On Windows, GLFW pulls in what it needs, but some setups like explicit OpenGL linkage:
if (WIN32)
  target_link_libraries(CleanRenderer PRIVATE opengl32)
endif()

# Copy shaders folder next to the exe (so relative paths work)
add_custom_command(TARGET CleanRenderer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:CleanRenderer>/shaders
)