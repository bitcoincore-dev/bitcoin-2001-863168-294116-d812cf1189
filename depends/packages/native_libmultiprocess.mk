package=native_libmultiprocess
$(package)_version=bd8ee26336fc269388ef8387ca3c854a4c3adca3
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=625bb189dece59dd81bcd5c0c77096b8c2bca38bcbf39aa5fa4a75b4ecdd6f83
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
