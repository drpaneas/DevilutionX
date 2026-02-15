# miniLZO - mini subset of the LZO real-time data compression library
# http://www.oberhumer.com/opensource/lzo/
#
# This is used for Dreamcast VMU saves which need compression to fit in ~100KB

# This file is only added via add_subdirectory when DREAMCAST is true,
# so no need for platform check here.

add_library(minilzo STATIC minilzo.c)

target_include_directories(minilzo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Suppress warnings for third-party code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(minilzo PRIVATE -w)
endif()
