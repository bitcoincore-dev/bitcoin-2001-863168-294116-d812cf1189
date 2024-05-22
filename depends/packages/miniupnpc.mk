package=miniupnpc
$(package)_version=2.2.7
$(package)_download_path=https://github.com/miniupnp/miniupnp/archive
$(package)_file_name=$(package)_$(subst .,_,$($(package)_version)).tar.gz
$(package)_sha256_hash=c57f3e78f9d2e97d62c0f50dc88cbed7e8c883d0364cae605a35595931301beb
$(package)_patches=dont_leak_info.patch cmake_get_src_addr.patch fix_windows_snprintf.patch
$(package)_build_subdir=build

define $(package)_set_vars
$(package)_config_opts = -DUPNPC_BUILD_SAMPLE=OFF -DUPNPC_BUILD_SHARED=OFF
$(package)_config_opts += -DUPNPC_BUILD_STATIC=ON -DUPNPC_BUILD_TESTS=OFF
$(package)_config_opts_mingw32 += -DMINIUPNPC_TARGET_WINDOWS_VERSION=0x0601
endef

define $(package)_extract_cmds
  mkdir -p $$($(1)_extract_dir) && \
  echo "$$($(1)_sha256_hash)  $$($(1)_source)" > $$($(1)_extract_dir)/.$$($(1)_file_name).hash && \
  $(build_SHA256SUM) -c $$($(1)_extract_dir)/.$$($(1)_file_name).hash && \
  $(build_TAR) --no-same-owner --strip-components=2 -xf $$($(1)_source) miniupnp-$(package)_$(subst .,_,$($(package)_version))/miniupnpc
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/dont_leak_info.patch && \
  patch -p1 < $($(package)_patch_dir)/cmake_get_src_addr.patch && \
  patch -p1 < $($(package)_patch_dir)/fix_windows_snprintf.patch
endef

define $(package)_config_cmds
  $($(package)_cmake) -S .. -B .
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  cmake --install . --prefix $($(package)_staging_prefix_dir)
endef
