// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <span.h>
#include <streams.h>

#include <array>

std::size_t XorFile::detail_fread(Span<std::byte> dst)
{
    if (!m_file) throw std::ios_base::failure("XorFile::read: file handle is nullptr");
    const auto init_pos{std::ftell(m_file)};
    if (init_pos < 0) return 0;
    std::size_t ret{std::fread(dst.data(), 1, dst.size(), m_file)};
    util::Xor(dst.subspan(0, ret), m_xor, init_pos);
    return ret;
}

void XorFile::read(Span<std::byte> dst)
{
    if (detail_fread(dst) != dst.size()) {
        throw std::ios_base::failure{feof() ? "XorFile::read: end of file" : "XorFile::read: failed"};
    }
}

void XorFile::ignore(size_t num_bytes)
{
    if (!m_file) throw std::ios_base::failure("XorFile::ignore: file handle is nullptr");
    std::array<std::byte, 4096> buf;
    while (num_bytes > 0) {
        auto buf_now{Span{buf}.first(std::min<size_t>(num_bytes, buf.size()))};
        if (std::fread(buf_now.data(), 1, buf_now.size(), m_file) != buf_now.size()) {
            throw std::ios_base::failure{feof() ? "XorFile::ignore: end of file" : "XorFile::ignore: failed"};
        }
        num_bytes -= buf_now.size();
    }
}

void XorFile::write(Span<const std::byte> src)
{
    if (!m_file) throw std::ios_base::failure("XorFile::write: file handle is nullptr");
    std::array<std::byte, 4096> buf;
    while (src.size() > 0) {
        auto buf_now{Span{buf}.first(std::min<size_t>(src.size(), buf.size()))};
        std::copy(src.begin(), src.begin() + buf_now.size(), buf_now.begin());
        const auto current_pos{std::ftell(m_file)};
        util::Xor(buf_now, m_xor, current_pos);
        if (current_pos < 0 || std::fwrite(buf_now.data(), 1, buf_now.size(), m_file) != buf_now.size()) {
            throw std::ios_base::failure{"XorFile::write: failed"};
        }
        src = src.subspan(buf_now.size());
    }
}

std::size_t AutoFile::detail_fread(Span<std::byte> dst)
{
    if (!file) throw std::ios_base::failure("AutoFile::read: file handle is nullptr");
    return std::fread(dst.data(), 1, dst.size(), file);
}
