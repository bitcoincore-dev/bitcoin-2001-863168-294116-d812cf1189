package := wayland
$(package)_version := 1.19.0
$(package)_download_path := https://wayland.freedesktop.org/releases
$(package)_file_name := $(package)-$($(package)_version).tar.xz
$(package)_sha256_hash := baccd902300d354581cd5ad3cc49daa4921d55fb416a5883e218750fef166d15
$(package)_dependencies := libffi expat

define $(package)_set_vars
  $(package)_config_opts := --enable-option-checking --disable-dependency-tracking
  $(package)_config_opts += --enable-shared --disable-static --disable-documentation
  $(package)_config_opts += --disable-dtd-validation
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf lib/*.la
endef
