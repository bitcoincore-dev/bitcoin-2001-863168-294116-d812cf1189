# A test project for the try_compile() command that checks
# whether a build succeeds without linking to the libssp library
# or passing the -fstack-protector-all option to a linker.

cmake_minimum_required(VERSION 3.16...3.28)

project(TestSSP CXX)

add_executable(test test.cpp)
target_compile_options(test PRIVATE -fstack-protector-all)
