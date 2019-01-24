// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <flatfile.h>
#include <logging.h>
#include <tinyformat.h>

FlatFileSeq::FlatFileSeq(fs::path dir, const char* prefix, size_t chunk_size)
    : m_dir(std::move(dir)), m_prefix(prefix), m_chunk_size(chunk_size)
{}

fs::path FlatFileSeq::FileName(const CDiskBlockPos& pos) const
{
    return m_dir / strprintf("%s%05u.dat", m_prefix, pos.nFile);
}

FILE* FlatFileSeq::Open(const CDiskBlockPos& pos, bool fReadOnly)
{
    if (pos.IsNull())
        return nullptr;
    fs::path path = FileName(pos);
    fs::create_directories(path.parent_path());
    FILE* file = fsbridge::fopen(path, fReadOnly ? "rb": "rb+");
    if (!file && !fReadOnly)
        file = fsbridge::fopen(path, "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return nullptr;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return nullptr;
        }
    }
    return file;
}
