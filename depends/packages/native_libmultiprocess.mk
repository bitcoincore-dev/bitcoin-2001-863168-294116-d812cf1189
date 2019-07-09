package=native_libmultiprocess
$(package)_version=5f425473be8925f161b3f04e31bbefc4abbb2581
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=f50f2f740d284690d906e0f19323fed3f89d2c27cc65787ea84f1f32fffba1c4
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
