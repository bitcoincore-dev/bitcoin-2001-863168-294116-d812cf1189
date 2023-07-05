// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <span.h>
#include <streams.h>

std::size_t AutoFile::detail_fread(Span<std::byte> dst)
{
    if (!file) throw std::ios_base::failure("AutoFile::read: file handle is nullptr");
    return std::fread(dst.data(), 1, dst.size(), file);
}
