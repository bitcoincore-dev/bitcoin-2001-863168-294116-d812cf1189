// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2023 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_FS_HELPERS_H
#define BITCOIN_UTIL_FS_HELPERS_H

#include <util/fs.h>

#include <cstdint>
#include <cstdio>
#include <iosfwd>
#include <limits>

//! Used to replace the implementation of the FileCommit function.
extern std::function<bool(FILE* file)> g_mock_file_commit;

/**
 * Ensure file contents are fully committed to disk, using a platform-specific
 * feature analogous to fsync().
 */
bool FileCommit(FILE* file);

//! Used to replace the implementation of the DirectoryCommit function.
extern std::function<void(const fs::path&)> g_mock_dir_commit;

/**
 * Sync directory contents. This is required on some environments to ensure that
 * newly created files are committed to disk.
 */
void DirectoryCommit(const fs::path& dirname);

//! Used to replace the implementation of the TruncateFile function.
extern std::function<bool(FILE*, unsigned int)> g_mock_truncate_file;

bool TruncateFile(FILE* file, unsigned int length);

int RaiseFileDescriptorLimit(int nMinFD);

//! Used to replace the implementation of the AllocateFileRange function.
extern std::function<void(FILE*, unsigned int, unsigned int)> g_mock_allocate_file_range;

void AllocateFileRange(FILE* file, unsigned int offset, unsigned int length);

/**
 * Rename src to dest.
 * @return true if the rename was successful.
 */
[[nodiscard]] bool RenameOver(fs::path src, fs::path dest);

namespace util {
enum class LockResult {
    Success,
    ErrorWrite,
    ErrorLock,
};
[[nodiscard]] LockResult LockDirectory(const fs::path& directory, const fs::path& lockfile_name, bool probe_only = false);
} // namespace util
void UnlockDirectory(const fs::path& directory, const fs::path& lockfile_name);

//! Used to replace the implementation of the CheckDiskSpace function.
extern std::function<bool(const fs::path&, uint64_t)> g_mock_check_disk_space;

bool CheckDiskSpace(const fs::path& dir, uint64_t additional_bytes = 0);

/** Get the size of a file by scanning it.
 *
 * @param[in] path The file path
 * @param[in] max Stop seeking beyond this limit
 * @return The file size or max
 */
std::streampos GetFileSize(const char* path, std::streamsize max = std::numeric_limits<std::streamsize>::max());

/** Release all directory locks. This is used for unit testing only, at runtime
 * the global destructor will take care of the locks.
 */
void ReleaseDirectoryLocks();

bool TryCreateDirectories(const fs::path& p);
fs::path GetDefaultDataDir();

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif

#endif // BITCOIN_UTIL_FS_HELPERS_H
