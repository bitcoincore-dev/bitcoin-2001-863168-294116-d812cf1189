package=libmultiprocess
$(package)_version := d576d975debdc9090bd2582f83f49c76c0061698
$(package)_download_path := https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name := $($(package)_version).tar.gz
$(package)_sha256_hash := 9f8b055c8bba755dc32fe799b67c20b91e7b13e67cadafbc54c0f1def057a370
$(package)_dependencies := capnp
ifneq ($(host),$(build))
$(package)_dependencies += native_capnp
endif

define $(package)_set_vars :=
ifneq ($(host),$(build))
$(package)_cmake_opts := -DCAPNP_EXECUTABLE="$(build_prefix)/bin/capnp"
$(package)_cmake_opts += -DCAPNPC_CXX_EXECUTABLE="$(build_prefix)/bin/capnpc-c++"
endif
endef

define $(package)_config_cmds
  $($(package)_cmake) .
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
