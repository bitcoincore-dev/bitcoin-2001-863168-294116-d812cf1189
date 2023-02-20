# Copyright (c) 2023 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

include(CheckIncludeFileCXX)
include(TestBigEndian)

test_big_endian(WORDS_BIGENDIAN)

# The following HAVE_{HEADER}_H variables go to the bitcoin-config.h header.
CHECK_INCLUDE_FILE_CXX(byteswap.h HAVE_BYTESWAP_H)
CHECK_INCLUDE_FILE_CXX(endian.h HAVE_ENDIAN_H)
CHECK_INCLUDE_FILE_CXX(sys/endian.h HAVE_SYS_ENDIAN_H)
CHECK_INCLUDE_FILE_CXX(sys/prctl.h HAVE_SYS_PRCTL_H)
CHECK_INCLUDE_FILE_CXX(sys/resources.h HAVE_SYS_RESOURCES_H)
CHECK_INCLUDE_FILE_CXX(sys/vmmeter.h HAVE_SYS_VMMETER_H)
CHECK_INCLUDE_FILE_CXX(vm/vm_param.h HAVE_VM_VM_PARAM_H)
