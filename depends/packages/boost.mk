package=boost
$(package)_version=1.80.0
$(package)_download_path=https://boostorg.jfrog.io/artifactory/main/beta/$($(package)_version).beta1/source/
$(package)_file_name=boost_$(subst .,_,$($(package)_version))_b1.tar.bz2
$(package)_sha256_hash=b29191c1bd4504511f9b1155c6f79f06cc7044444780392ed7358f5778ac7d16

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  cp -r boost $($(package)_staging_prefix_dir)/include
endef
