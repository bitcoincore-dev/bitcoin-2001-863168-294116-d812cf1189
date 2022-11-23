package=boost
$(package)_version=1.81.0
$(package)_download_path=https://boostorg.jfrog.io/artifactory/main/beta/$($(package)_version).beta1/source/
$(package)_file_name=boost_$(subst .,_,$($(package)_version))_b1.tar.bz2
$(package)_sha256_hash=941c568e7ac79aa448ac28c98a5ec391fd1317170953c487bcf977c6ee6061ce

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/include && \
  cp -r boost $($(package)_staging_prefix_dir)/include
endef
