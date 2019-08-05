package=native_libmultiprocess
$(package)_version=9cd1a5a9c1d73d7f328b46daf27b4d5717537824
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=7cebe2c3d14251e035faf1b0e7eae080799ab6c857fbc641c162fdfe8407148f
$(package)_dependencies=native_boost native_capnp

define $(package)_config_cmds
  cmake -DCMAKE_INSTALL_PREFIX=$($($(package)_type)_prefix)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
