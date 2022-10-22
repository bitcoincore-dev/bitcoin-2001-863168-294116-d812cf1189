# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Check for clmul instructions support.
if(MSVC)
  set(CLMUL_CXXFLAGS)
else()
  set(CLMUL_CXXFLAGS -mpclmul)
endif()
set(CMAKE_REQUIRED_FLAGS ${CLMUL_CXXFLAGS})
check_cxx_source_compiles("
  #include <immintrin.h>
  #include <cstdint>

  int main()
  {
    __m128i a = _mm_cvtsi64_si128((uint64_t)7);
    __m128i b = _mm_clmulepi64_si128(a, a, 37);
    __m128i c = _mm_srli_epi64(b, 41);
    __m128i d = _mm_xor_si128(b, c);
    uint64_t e = _mm_cvtsi128_si64(d);
    return e == 0;
  }
  " HAVE_CLMUL
)
set(CMAKE_REQUIRED_FLAGS)

# Check for working clz builtins.
check_cxx_source_compiles("
  int main()
  {
    unsigned a = __builtin_clz(1);
    unsigned long b = __builtin_clzl(1);
    unsigned long long c = __builtin_clzll(1);
  }
  " HAVE_CLZ
)

add_library(minisketch STATIC EXCLUDE_FROM_ALL
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/minisketch.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_1byte.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_2bytes.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_3bytes.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_4bytes.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_5bytes.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_6bytes.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_7bytes.cpp
  ${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/generic_8bytes.cpp
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_1byte.cpp>
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_2bytes.cpp>
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_3bytes.cpp>
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_4bytes.cpp>
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_5bytes.cpp>
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_6bytes.cpp>
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_7bytes.cpp>
  $<$<BOOL:${HAVE_CLMUL}>:${PROJECT_SOURCE_DIR}/src/minisketch/src/fields/clmul_8bytes.cpp>
)

target_compile_definitions(minisketch
  PRIVATE
    DISABLE_DEFAULT_FIELDS
    ENABLE_FIELD_32
    $<$<BOOL:${HAVE_CLMUL}>:HAVE_CLMUL>
    $<$<BOOL:${HAVE_CLZ}>:HAVE_CLZ>
)

target_include_directories(minisketch
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/minisketch/include>
)

target_compile_options(minisketch
  PRIVATE
    $<$<BOOL:${HAVE_CLMUL}>:${CLMUL_CXXFLAGS}>
    $<$<CXX_COMPILER_ID:MSVC>:/wd4060 /wd4065 /wd4146>
)
