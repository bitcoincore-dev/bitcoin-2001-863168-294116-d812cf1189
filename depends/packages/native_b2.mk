package=native_b2
$(package)_version=1_71_0
$(package)_download_path=https://boostorg.jfrog.io/artifactory/main/release/$(subst _,.,$($(package)_version))/source/
$(package)_file_name=boost_$($(package)_version).tar.bz2
$(package)_sha256_hash=d73a8da01e8bf8c7eda40b4c84915071a8c8a0df4a6734537ddde4a8580524ee

$(package)_build_subdir=tools/build/src/engine
ifneq (,$(findstring clang,$($(package)_cxx)))
$(package)_toolset_$(host_os)=clang
else
$(package)_toolset_$(host_os)=gcc
endif

define $(package)_build_cmds
  CXX="$($(package)_cxx)" CXXFLAGS="$($(package)_cxxflags)" ./build.sh "$($(package)_toolset_$(host_os))"
endef

define $(package)_stage_cmds
  mkdir -p "$($(package)_staging_prefix_dir)"/bin/ && \
  cp b2 "$($(package)_staging_prefix_dir)"/bin/
endef
