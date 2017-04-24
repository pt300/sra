cmake_minimum_required(VERSION 2.8)
set(CMAKE_SYSTEM_NAME Windows)
project(sra C)

set(CMAKE_C_STANDARD 99)

enable_language(RC)
add_definitions(-DUNICODE -D_UNICODE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
#find_package(FFTW3 REQUIRED)

add_executable(sra WIN32 main resource.rc)
target_link_libraries(sra gdi32 winmm ksuser comctl32 gdi32)
#target_link_libraries(sra ${FFTW3_FFTWF_LIBRARY})
#didn't feel like fighting with cmake to link against the static library
target_link_libraries(sra /usr/i686-w64-mingw32/lib/libfftw3f.a)
