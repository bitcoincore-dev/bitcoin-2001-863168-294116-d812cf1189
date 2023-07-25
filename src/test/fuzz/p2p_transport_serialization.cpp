// Copyright (c) 2019-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <hash.h>
#include <net.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/util/xoroshiro128plusplus.h>
#include <util/chaintype.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

void initialize_p2p_transport_serialization()
{
    SelectParams(ChainType::REGTEST);
}

FUZZ_TARGET(p2p_transport_serialization, .init = initialize_p2p_transport_serialization)
{
    // Construct transports for both sides, with dummy NodeIds.
    V1Transport recv_transport{Params(), NodeId{0}, SER_NETWORK, INIT_PROTO_VERSION};
    V1Transport send_transport{Params(), NodeId{1}, SER_NETWORK, INIT_PROTO_VERSION};

    FuzzedDataProvider fuzzed_data_provider{buffer.data(), buffer.size()};

    auto checksum_assist = fuzzed_data_provider.ConsumeBool();
    auto magic_bytes_assist = fuzzed_data_provider.ConsumeBool();
    std::vector<uint8_t> mutable_msg_bytes;

    auto header_bytes_remaining = CMessageHeader::HEADER_SIZE;
    if (magic_bytes_assist) {
        auto msg_start = Params().MessageStart();
        for (size_t i = 0; i < CMessageHeader::MESSAGE_SIZE_SIZE; ++i) {
            mutable_msg_bytes.push_back(msg_start[i]);
        }
        header_bytes_remaining -= CMessageHeader::MESSAGE_SIZE_SIZE;
    }

    if (checksum_assist) {
        header_bytes_remaining -= CMessageHeader::CHECKSUM_SIZE;
    }

    auto header_random_bytes = fuzzed_data_provider.ConsumeBytes<uint8_t>(header_bytes_remaining);
    mutable_msg_bytes.insert(mutable_msg_bytes.end(), header_random_bytes.begin(), header_random_bytes.end());
    auto payload_bytes = fuzzed_data_provider.ConsumeRemainingBytes<uint8_t>();

    if (checksum_assist && mutable_msg_bytes.size() == CMessageHeader::CHECKSUM_OFFSET) {
        CHash256 hasher;
        unsigned char hsh[32];
        hasher.Write(payload_bytes);
        hasher.Finalize(hsh);
        for (size_t i = 0; i < CMessageHeader::CHECKSUM_SIZE; ++i) {
           mutable_msg_bytes.push_back(hsh[i]);
        }
    }

    mutable_msg_bytes.insert(mutable_msg_bytes.end(), payload_bytes.begin(), payload_bytes.end());
    Span<const uint8_t> msg_bytes{mutable_msg_bytes};
    while (msg_bytes.size() > 0) {
        const int handled = recv_transport.Read(msg_bytes);
        if (handled < 0) {
            break;
        }
        if (recv_transport.Complete()) {
            const std::chrono::microseconds m_time{std::numeric_limits<int64_t>::max()};
            bool reject_message{false};
            CNetMessage msg = recv_transport.GetMessage(m_time, reject_message);
            assert(msg.m_type.size() <= CMessageHeader::COMMAND_SIZE);
            assert(msg.m_raw_message_size <= mutable_msg_bytes.size());
            assert(msg.m_raw_message_size == CMessageHeader::HEADER_SIZE + msg.m_message_size);
            assert(msg.m_time == m_time);

            std::vector<unsigned char> header;
            auto msg2 = CNetMsgMaker{msg.m_recv.GetVersion()}.Make(msg.m_type, Span{msg.m_recv});
            assert(send_transport.DoneSendingMessage());
            send_transport.SetMessageToSend(std::move(msg2));
            while (true) {
                bool have_bytes = send_transport.HaveBytesToSend();
                const auto& [to_send, more, _msg_type] = send_transport.GetBytesToSend();
                assert(have_bytes == !to_send.empty());
                if (to_send.empty()) break;
                send_transport.MarkBytesSent(to_send.size());
                if (!more) assert(send_transport.HaveBytesToSend() == 0);
            }
            assert(send_transport.DoneSendingMessage());
        }
    }
}

FUZZ_TARGET(p2p_transport_bidirectional, .init = initialize_p2p_transport_serialization)
{
    // Simulation test with two V1Transport objects, which send messages to each other, with
    // sending and receiving fragmented into multiple pieces that may be interleaved. It primarily
    // verifies that the sending and receiving side are compatible with each other, plus a few
    // sanity checks.

    FuzzedDataProvider provider{buffer.data(), buffer.size()};
    XoRoShiRo128PlusPlus rng(provider.ConsumeIntegral<uint64_t>());

    // Construct two transports, each representing one side.
    std::array<V1Transport, 2> transports = {
        V1Transport{Params(), NodeId{0}, SER_NETWORK, INIT_PROTO_VERSION},
        V1Transport{Params(), NodeId{1}, SER_NETWORK, INIT_PROTO_VERSION}
    };

    // Two deques representing in-flight bytes. inflight[i] is from transport[i] to transport[!i].
    std::array<std::vector<uint8_t>, 2> in_flight;

    // Two deques with expected messages. expected[i] is expected to arrive in transport[!i].
    std::array<std::deque<CSerializedNetMsg>, 2> expected;

    // Function to consume a message type.
    auto msg_type_fn = [&]() {
        std::string ret;
        while (ret.size() < CMessageHeader::COMMAND_SIZE) {
            char c = provider.ConsumeIntegral<char>();
            if (c < ' ' || c > 0x7E) break;
            ret += c;
        }
        return ret;
    };

    // Function to make side send a new message.
    auto new_msg_fn = [&](int side) {
        // Don't do anything if there is an enqueued message already.
        if (!transports[side].DoneSendingMessage()) return false;
        // Don't do anything if there are too many unreceived messages already.
        if (expected[side].size() >= 16) return false;
        // Determine size of message to send.
        size_t size = provider.ConsumeIntegralInRange<uint32_t>(0, 75000); // Limit to 75 KiB for performance reasons
        // Construct a message to send.
        CSerializedNetMsg msg;
        msg.m_type = msg_type_fn();
        msg.data.resize(size);
        for (auto& v : msg.data) v = uint8_t(rng());
        expected[side].emplace_back(msg.Copy());
        transports[side].SetMessageToSend(std::move(msg));
        return true;
    };

    // Function to make side send out bytes (if any).
    auto send_fn = [&](int side, bool everything = false) {
        bool have_any = transports[side].HaveBytesToSend();
        const auto& [bytes, more, msg_type] = transports[side].GetBytesToSend();
        assert(have_any == !bytes.empty());
        // Don't do anything if no bytes to send.
        if (!have_any) return false;
        size_t send_now = everything ? bytes.size() : provider.ConsumeIntegralInRange<size_t>(0, bytes.size());
        if (send_now == 0) return false;
        in_flight[side].insert(in_flight[side].end(), bytes.begin(), bytes.begin() + send_now);
        transports[side].MarkBytesSent(send_now);
        return send_now > 0;
    };

    // Function to make !side receive bytes (if any).
    auto recv_fn = [&](int side, bool everything = false) {
        // Don't do anything if no bytes in flight.
        if (in_flight[side].empty()) return false;
        // Decide span to receive
        size_t to_recv_len = in_flight[side].size();
        if (!everything) to_recv_len = provider.ConsumeIntegralInRange<size_t>(0, to_recv_len);
        Span<const uint8_t> to_recv = Span{in_flight[side]}.first(to_recv_len);
        // Process those bytes
        while (!to_recv.empty()) {
            int ret = transports[!side].Read(to_recv);
            assert(ret >= 0);
            bool progress = ret > 0;
            if (transports[!side].Complete()) {
                bool reject{false};
                auto received = transports[!side].GetMessage({}, reject);
                // Receiving must succeed.
                assert(!reject);
                // There must be a corresponding expected message.
                assert(!expected[side].empty());
                // The m_message_size field must be correct.
                assert(received.m_message_size == received.m_recv.size());
                // The m_type must match what is expected.
                assert(received.m_type == expected[side].front().m_type);
                // The data must match what is expected.
                assert(MakeByteSpan(received.m_recv) == MakeByteSpan(expected[side].front().data));
                expected[side].pop_front();
                progress = true;
            }
            assert(progress);
        }
        // Remove those bytes from the in_flight buffer.
        in_flight[side].erase(in_flight[side].begin(), in_flight[side].begin() + to_recv_len);
        return to_recv_len > 0;
    };

    // Main loop, interleaving new messages, sends, and receives.
    unsigned iter = 0;
    while (iter < 1000) {
        uint8_t ops = provider.ConsumeIntegral<uint8_t>();
        // Stop at 0.
        if (ops == 0) break;
        // The first 6 bits of ops identify which operations are attempted.
        if (ops & 0x01) new_msg_fn(0);
        if (ops & 0x02) new_msg_fn(1);
        if (ops & 0x04) send_fn(0);
        if (ops & 0x08) send_fn(1);
        if (ops & 0x10) recv_fn(0);
        if (ops & 0x20) recv_fn(1);
        ++iter;
    }

    // When we're done, perform sends and receives of existing messages to flush anything already
    // in flight.
    while (true) {
        bool any = false;
        if (send_fn(0, true)) any = true;
        if (send_fn(1, true)) any = true;
        if (recv_fn(0, true)) any = true;
        if (recv_fn(1, true)) any = true;
        if (!any) break;
    }

    // Make sure nothing is left in flight.
    assert(in_flight[0].empty());
    assert(in_flight[1].empty());

    // Make sure all expected messages were received.
    assert(expected[0].empty());
    assert(expected[1].empty());
}
