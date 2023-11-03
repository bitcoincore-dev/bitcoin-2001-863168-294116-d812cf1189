# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

if(ASM AND NOT MSVC)
  include(CheckSourceCompilesAndLinks)

  # Check for SSE4.1 intrinsics.
  set(SSE41_CXXFLAGS -msse4.1)
  check_cxx_source_compiles_with_flags("${SSE41_CXXFLAGS}" "
    #include <immintrin.h>

    int main()
    {
      __m128i l = _mm_set1_epi32(0);
      return _mm_extract_epi32(l, 3);
    }
    " HAVE_SSE41
  )

  # Check for AVX2 intrinsics.
  set(AVX2_CXXFLAGS -mavx -mavx2)
  check_cxx_source_compiles_with_flags("${AVX2_CXXFLAGS}" "
    #include <immintrin.h>

    int main()
    {
      __m256i l = _mm256_set1_epi32(0);
      return _mm256_extract_epi32(l, 7);
    }
    " HAVE_AVX2
  )

  # Check for x86 SHA-NI intrinsics.
  set(X86_SHANI_CXXFLAGS -msse4 -msha)
  check_cxx_source_compiles_with_flags("${X86_SHANI_CXXFLAGS}" "
    #include <immintrin.h>

    int main()
    {
      __m128i i = _mm_set1_epi32(0);
      __m128i j = _mm_set1_epi32(1);
      __m128i k = _mm_set1_epi32(2);
      return _mm_extract_epi32(_mm_sha256rnds2_epu32(i, j, k), 0);
    }
    " HAVE_X86_SHANI
  )

  # Check for ARMv8 SHA-NI intrinsics.
  set(ARM_SHANI_CXXFLAGS -march=armv8-a+crypto)
  check_cxx_source_compiles_with_flags("${ARM_SHANI_CXXFLAGS}" "
    #include <arm_neon.h>

    int main()
    {
      uint32x4_t a, b, c;
      vsha256h2q_u32(a, b, c);
      vsha256hq_u32(a, b, c);
      vsha256su0q_u32(a, b);
      vsha256su1q_u32(a, b, c);
    }
    " HAVE_ARM_SHANI
  )
endif()

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(timingsafe_bcmp "string.h" HAVE_TIMINGSAFE_BCMP)

if(HAVE_SSE41)
  add_library(bitcoin_crypto_sse41 OBJECT EXCLUDE_FROM_ALL
    sha256_sse41.cpp
  )
  target_compile_definitions(bitcoin_crypto_sse41
    PUBLIC
      ENABLE_SSE41
  )
  target_compile_options(bitcoin_crypto_sse41
    PRIVATE
      ${SSE41_CXXFLAGS}
  )
  target_link_libraries(bitcoin_crypto_sse41 PRIVATE core)
endif()

if(HAVE_AVX2)
  add_library(bitcoin_crypto_avx2 OBJECT EXCLUDE_FROM_ALL
    sha256_avx2.cpp
  )
  target_compile_definitions(bitcoin_crypto_avx2
    PUBLIC
      ENABLE_AVX2
  )
  target_compile_options(bitcoin_crypto_avx2
    PRIVATE
      ${AVX2_CXXFLAGS}
  )
  target_link_libraries(bitcoin_crypto_avx2 PRIVATE core)
endif()

if(HAVE_X86_SHANI)
  add_library(bitcoin_crypto_x86_shani OBJECT EXCLUDE_FROM_ALL
    sha256_x86_shani.cpp
  )
  target_compile_definitions(bitcoin_crypto_x86_shani
    PUBLIC
      ENABLE_X86_SHANI
  )
  target_compile_options(bitcoin_crypto_x86_shani
    PRIVATE
      ${X86_SHANI_CXXFLAGS}
  )
  target_link_libraries(bitcoin_crypto_x86_shani PRIVATE core)
endif()

if(HAVE_ARM_SHANI)
  add_library(bitcoin_crypto_arm_shani OBJECT EXCLUDE_FROM_ALL
    sha256_arm_shani.cpp
  )
  target_compile_definitions(bitcoin_crypto_arm_shani
    PUBLIC
      ENABLE_ARM_SHANI
  )
  target_compile_options(bitcoin_crypto_arm_shani
    PRIVATE
      ${ARM_SHANI_CXXFLAGS}
  )
  target_link_libraries(bitcoin_crypto_arm_shani PRIVATE core)
endif()

# These sources are to be compiled with different compile definitions.
set(bitcoin_crypto_base_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/aes.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/chacha20.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/chacha20poly1305.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hkdf_sha256_32.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hmac_sha256.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hmac_sha512.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/poly1305.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/muhash.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ripemd160.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sha1.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sha256.cpp
  $<$<BOOL:${ASM}>:${CMAKE_CURRENT_SOURCE_DIR}/sha256_sse4.cpp>
  ${CMAKE_CURRENT_SOURCE_DIR}/sha3.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sha512.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/siphash.cpp
)
set(bitcoin_crypto_base_sources ${bitcoin_crypto_base_sources} PARENT_SCOPE)

add_library(bitcoin_crypto STATIC EXCLUDE_FROM_ALL
  ${bitcoin_crypto_base_sources}
)
target_compile_definitions(bitcoin_crypto
  PRIVATE
    $<$<BOOL:${ASM}>:USE_ASM>
    $<$<BOOL:${HAVE_TIMINGSAFE_BCMP}>:HAVE_TIMINGSAFE_BCMP>
)
target_link_libraries(bitcoin_crypto
  PRIVATE
    core
    $<TARGET_NAME_IF_EXISTS:bitcoin_crypto_sse41>
    $<TARGET_NAME_IF_EXISTS:bitcoin_crypto_avx2>
    $<TARGET_NAME_IF_EXISTS:bitcoin_crypto_x86_shani>
    $<TARGET_NAME_IF_EXISTS:bitcoin_crypto_arm_shani>
)
