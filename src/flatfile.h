// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_FLATFILE_H
#define BITCOIN_FLATFILE_H

#include <chain.h>
#include <fs.h>

/**
 * FlatFileSeq represents a sequence of numbered files storing raw data. This class facilitates
 * access to and efficient management of these files.
 */
class FlatFileSeq
{
private:
    fs::path m_dir;
    const char* m_prefix;
    size_t m_chunk_size;

public:
    /**
     * Constructor
     *
     * @param dir The base directory that all files live in.
     * @param prefix A short prefix given to all file names.
     * @param chunk_size Disk space is pre-allocated in multiples of this amount.
     */
    FlatFileSeq(fs::path dir, const char* prefix, size_t chunk_size);

    /** Get the name of the file at the given position. */
    fs::path FileName(const CDiskBlockPos& pos) const;

    /** Open a handle to the file at the given position. */
    FILE* Open(const CDiskBlockPos& pos, bool read_only = false);
};

#endif // BITCOIN_FLATFILE_H
