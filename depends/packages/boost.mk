package=boost
$(package)_version=1.80.0
$(package)_download_path=https://boostorg.jfrog.io/artifactory/main/release/$($(package)_version)/source/
$(package)_file_name=boost_$(subst .,_,$($(package)_version)).tar.bz2
$(package)_sha256_hash=1e19565d82e43bc59209a168f5ac899d3ba471d55c7610c677d4ccf2c9c500c0
$(package)_patches := mingw32_process.patch

define $(package)_preprocess_cmds
  patch -p1 -i $($(package)_patch_dir)/mingw32_process.patch
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  cp -r boost $($(package)_staging_prefix_dir)/include
endef
