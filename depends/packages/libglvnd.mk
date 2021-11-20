package := libglvnd
$(package)_version := 1.3.4
$(package)_download_path := https://gitlab.freedesktop.org/glvnd/$(package)/-/archive/v$($(package)_version)
$(package)_file_name := $(package)-v$($(package)_version).tar.bz2
$(package)_sha256_hash := daa5de961aaaec7c1883c09db7748a7f221116a942d1397b35655db92ad4efb0

define $(package)_set_vars
  $(package)_config_opts := --enable-option-checking --disable-dependency-tracking
  $(package)_config_opts += --enable-shared --disable-static
  $(package)_config_opts += --disable-x11 --disable-gles1
endef

define $(package)_config_cmds
  ./autogen.sh && \
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm lib/*.la
endef
