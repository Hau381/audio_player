cmake_minimum_required(VERSION 3.16)
project(hello)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# Use vendored libs
set(SDLTTF_VENDORED ON)

# This assumes the SDL source is available in vendored/SDL
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# This assumes the SDL_ttf source is available in vendored/SDL_ttf
add_subdirectory(SDL_ttf EXCLUDE_FROM_ALL)

# Create your game executable target as usual
add_executable(hello WIN32 src/audio_player.c)

# Link to the actual SDL3 library.
target_link_libraries(hello PRIVATE SDL3_ttf::SDL3_ttf SDL3::SDL3)