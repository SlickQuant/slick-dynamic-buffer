/********************************************************************************
 * Copyright (c) 2026 Slick Quant LLC
 * All rights reserved
 *
 * This file is part of the SlickStreamBuffer. Redistribution and use in source
 * and binary forms, with or without modification, are permitted exclusively
 * under the terms of the MIT license which is available at
 * https://github.com/SlickQuant/slick-stream-buffer/blob/main/LICENSE
 *
 ********************************************************************************/

#include <gtest/gtest.h>

// Boost must come before slick-queue and slick-stream-buffer-multiplexer to avoid
// preprocessor collisions from Windows headers pulled in by the shm/atomic paths.
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <slick/dynamic_buffer.hpp>
#include <slick/stream_buffer.hpp>
#include <slick/stream_buffer_multiplexer.hpp>

#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace net = boost::asio;
using net::ip::tcp;
using slick::stream_buffer;
using slick::dynamic_buffer;
using slick::stream_buffer_multiplexer;

// The whole point of the adapter: beast/asio must accept it as a DynamicBuffer.
// (Modern Beast uses asio's DynamicBuffer_v1 trait as its DynamicBuffer concept.)
static_assert(net::is_dynamic_buffer_v1<dynamic_buffer<stream_buffer>>::value,
              "dynamic_buffer<stream_buffer> must satisfy asio's DynamicBuffer_v1 requirements");
static_assert(net::is_dynamic_buffer_v1<dynamic_buffer<stream_buffer_multiplexer::producer_buffer>>::value,
              "dynamic_buffer<producer_buffer> must satisfy asio's DynamicBuffer_v1 requirements");

TEST(DynamicBufferTests, BufferCopyRoundtrip) {
    stream_buffer sb(1024, 16);
    dynamic_buffer dyn(sb);

    const std::string src = "dynamic buffer roundtrip";
    const std::size_t copied = net::buffer_copy(dyn.prepare(src.size()), net::buffer(src));
    ASSERT_EQ(copied, src.size());
    dyn.commit(copied);

    EXPECT_EQ(dyn.size(), src.size());
    auto readable = dyn.data();
    ASSERT_EQ(readable.size(), src.size());
    EXPECT_EQ(std::memcmp(readable.data(), src.data(), src.size()), 0);

    auto record = dyn.consume(src.size());
    EXPECT_EQ(dyn.size(), 0u);

    // consume() returns the published record and the bytes are readable on the
    // underlying stream buffer - both views are the same memory
    ASSERT_TRUE(static_cast<bool>(record));
    EXPECT_EQ(record.length, src.size());

    uint64_t cursor = 0;
    auto [ptr, len] = dyn.buffer().read(cursor);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(len, src.size());
    EXPECT_EQ(std::memcmp(ptr, src.data(), len), 0);
    EXPECT_EQ(ptr, record.data);
}

TEST(DynamicBufferTests, MaxSizeClampsAndThrows) {
    stream_buffer sb(1024, 16);
    dynamic_buffer dyn(sb, 16);

    EXPECT_EQ(dyn.max_size(), 16u);
    EXPECT_NO_THROW(dyn.prepare(16));
    EXPECT_THROW(dyn.prepare(17), std::length_error);

    net::buffer_copy(dyn.prepare(10), net::buffer(std::string(10, 'x')));
    dyn.commit(10);
    EXPECT_THROW(dyn.prepare(7), std::length_error);   // 10 committed + 7 > 16
    EXPECT_NO_THROW(dyn.prepare(6));

    // max_size is clamped to the ring capacity
    dynamic_buffer dyn2(sb);
    EXPECT_EQ(dyn2.max_size(), sb.capacity());
}

TEST(DynamicBufferTests, DataRespectsMaxSizeWhenUnderlyingBufferIsLarger) {
    stream_buffer sb(1024, 16);
    dynamic_buffer dyn(sb, 4);

    auto [ptr, sz] = sb.prepare(8);
    ASSERT_EQ(sz, 8u);
    std::memcpy(ptr, "abcdefgh", 8);
    sb.commit(8);

    EXPECT_EQ(dyn.size(), 4u);
    auto readable = dyn.data();
    EXPECT_EQ(readable.size(), 4u);
    EXPECT_EQ(std::memcmp(readable.data(), "abcd", 4), 0);
}

TEST(DynamicBufferTests, ClearDiscardsPartialMessageOnDisconnect) {
    stream_buffer sb(1024, 16);
    dynamic_buffer dyn(sb);
    uint64_t cursor = 0;

    // first connection: one complete message published, then a partial read
    // committed before the connection drops
    const std::string complete = "complete-msg";
    net::buffer_copy(dyn.prepare(complete.size()), net::buffer(complete));
    dyn.commit(complete.size());
    dyn.consume(complete.size());

    const std::string partial = "part";
    net::buffer_copy(dyn.prepare(partial.size()), net::buffer(partial));
    dyn.commit(partial.size());
    ASSERT_EQ(dyn.size(), partial.size());

    // disconnect: the partial message is invalid for the next connection
    dyn.clear();
    EXPECT_EQ(dyn.size(), 0u);

    // the record published before the disconnect is still readable here because
    // the discarded bytes did not wrap over it
    auto [p0, l0] = sb.read(cursor);
    ASSERT_NE(p0, nullptr);
    ASSERT_EQ(l0, complete.size());
    EXPECT_EQ(std::memcmp(p0, complete.data(), l0), 0);

    // reconnect: the new message must not contain the discarded bytes
    const std::string fresh = "fresh-msg";
    net::buffer_copy(dyn.prepare(fresh.size()), net::buffer(fresh));
    dyn.commit(fresh.size());
    auto record = dyn.consume(fresh.size());
    ASSERT_TRUE(static_cast<bool>(record));
    ASSERT_EQ(record.length, fresh.size());
    EXPECT_EQ(std::memcmp(record.data, fresh.data(), record.length), 0);

    auto [p1, l1] = sb.read(cursor);
    ASSERT_NE(p1, nullptr);
    ASSERT_EQ(l1, fresh.size());
    EXPECT_EQ(std::memcmp(p1, fresh.data(), l1), 0);
}

TEST(DynamicBufferTests, TcpLoopbackZeroCopyRead) {
    net::io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
    acceptor.listen();

    tcp::socket writer(ioc);
    writer.connect(tcp::endpoint(net::ip::make_address("127.0.0.1"), acceptor.local_endpoint().port()));
    tcp::socket reader = acceptor.accept();

    // length-prefixed messages, written as one stream
    const std::vector<std::string> messages = { "alpha", "bravo charlie", "d", "echo foxtrot golf" };
    std::string stream_bytes;
    for (const auto& m : messages) {
        uint32_t len = static_cast<uint32_t>(m.size());
        stream_bytes.append(reinterpret_cast<const char*>(&len), sizeof(len));
        stream_bytes.append(m);
    }
    net::write(writer, net::buffer(stream_bytes));

    // receive into the stream buffer and publish one record per complete message
    stream_buffer sb(1 << 12, 64);
    dynamic_buffer dyn(sb);
    uint64_t cursor = 0;
    std::size_t verified = 0;

    while (verified < messages.size()) {
        const std::size_t n = reader.read_some(dyn.prepare(1024));
        dyn.commit(n);

        // parse: publish every complete length-prefixed message
        for (;;) {
            const std::size_t readable = dyn.size();
            if (readable < sizeof(uint32_t)) break;
            uint32_t len;
            std::memcpy(&len, dyn.data().data(), sizeof(len));
            if (readable < sizeof(len) + len) break;
            dyn.consume(sizeof(len) + len);  // publishes prefix + payload as one record
        }

        // drain published records and verify against the source messages (zero-copy)
        for (;;) {
            auto [ptr, len] = sb.read(cursor);
            if (ptr == nullptr) break;
            ASSERT_LT(verified, messages.size());
            const std::string& expected = messages[verified];
            ASSERT_EQ(len, sizeof(uint32_t) + expected.size());
            uint32_t payload_len;
            std::memcpy(&payload_len, ptr, sizeof(payload_len));
            ASSERT_EQ(payload_len, expected.size());
            EXPECT_EQ(std::memcmp(ptr + sizeof(payload_len), expected.data(), expected.size()), 0);
            ++verified;
        }
    }
    EXPECT_EQ(verified, messages.size());
    EXPECT_EQ(sb.loss_count(), 0u);
}

TEST(DynamicBufferTests, AsioReadComposedOp) {
    net::io_context ioc;
    tcp::acceptor acceptor(ioc, tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
    acceptor.listen();

    tcp::socket writer(ioc);
    writer.connect(tcp::endpoint(net::ip::make_address("127.0.0.1"), acceptor.local_endpoint().port()));
    tcp::socket reader = acceptor.accept();

    const std::string payload(2000, '\0');
    std::string src = payload;
    for (std::size_t i = 0; i < src.size(); ++i) src[i] = static_cast<char>(i & 0xff);
    net::write(writer, net::buffer(src));

    // asio's composed read copies the DynamicBuffer_v1 by value internally -
    // this proves the adapter survives that
    stream_buffer sb(1 << 12, 16);
    dynamic_buffer dyn(sb);
    const std::size_t n = net::read(reader, dyn, net::transfer_exactly(src.size()));
    ASSERT_EQ(n, src.size());
    ASSERT_EQ(dyn.size(), src.size());
    EXPECT_EQ(std::memcmp(dyn.data().data(), src.data(), src.size()), 0);

    dyn.consume(src.size());
    uint64_t cursor = 0;
    auto [ptr, len] = sb.read(cursor);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(len, src.size());
    EXPECT_EQ(std::memcmp(ptr, src.data(), len), 0);
}

TEST(DynamicBufferTests, ProducerBufferBackendRoundtrip) {
    stream_buffer_multiplexer mux(64);
    auto pb = mux.add_producer(0, 1024, 16);  // shared_ptr<producer_buffer>

    // shared_ptr constructor (owning) — pb stays alive as long as dyn does
    dynamic_buffer dyn(pb);

    constexpr std::string_view msg = "mpmc test";
    const std::size_t copied = net::buffer_copy(dyn.prepare(msg.size()), net::buffer(msg.data(), msg.size()));
    ASSERT_EQ(copied, msg.size());
    dyn.commit(copied);
    EXPECT_EQ(dyn.size(), msg.size());

    // consume() publishes into the multiplexer's shared queue via producer_buffer
    auto rec = dyn.consume(msg.size());
    ASSERT_TRUE(static_cast<bool>(rec));
    EXPECT_EQ(rec.length, msg.size());
    EXPECT_EQ(std::memcmp(rec.data, msg.data(), msg.size()), 0);

    // multiplexer consumer can read it back
    uint64_t cursor = 0;
    auto mrec = mux.read(cursor);
    ASSERT_TRUE(mrec);
    EXPECT_EQ(mrec.producer_id, 0u);
    EXPECT_EQ(mrec.length, msg.size());
    EXPECT_EQ(std::memcmp(mrec.data, msg.data(), msg.size()), 0);
    EXPECT_EQ(mux.loss_count(), 0u);
}

TEST(DynamicBufferTests, ProducerBufferBufferPtrSharesOwnership) {
    stream_buffer_multiplexer mux(64);
    auto pb = mux.add_producer(0, 1024, 16);

    dynamic_buffer dyn(pb);
    auto ptr = dyn.buffer_ptr();  // shared_ptr to same producer_buffer
    EXPECT_EQ(ptr.get(), pb.get());
    EXPECT_GT(ptr.use_count(), 1);  // dyn + pb + ptr all share
}

// Regression: prepare(n) used to compute size() + n, which wraps for a huge n and
// slipped past the max_size cap, handing back a writable region the ring cannot hold.
TEST(DynamicBufferTests, PrepareRejectsSizeOverflow) {
    stream_buffer sb(1024, 16);
    dynamic_buffer dyn(sb);
    ASSERT_EQ(dyn.max_size(), sb.capacity());

    constexpr std::size_t max_n = (std::numeric_limits<std::size_t>::max)();
    EXPECT_THROW(dyn.prepare(max_n), std::length_error);

    // one committed byte is what makes the old check wrap to 0
    net::buffer_copy(dyn.prepare(1), net::buffer(std::string(1, 'a')));
    dyn.commit(1);
    ASSERT_EQ(dyn.size(), 1u);
    EXPECT_THROW(dyn.prepare(max_n), std::length_error);
    EXPECT_THROW(dyn.prepare(max_n - 1), std::length_error);
    EXPECT_THROW(dyn.prepare(dyn.max_size()), std::length_error);

    // the exact remaining room is still accepted
    EXPECT_NO_THROW(dyn.prepare(dyn.max_size() - dyn.size()));
}

// Same overflow, on a capped adapter: the cap has to hold whatever n is passed.
TEST(DynamicBufferTests, PrepareRejectsSizeOverflowWhenCapped) {
    stream_buffer sb(1024, 16);
    dynamic_buffer dyn(sb, 16);

    net::buffer_copy(dyn.prepare(1), net::buffer(std::string(1, 'a')));
    dyn.commit(1);
    EXPECT_THROW(dyn.prepare((std::numeric_limits<std::size_t>::max)()), std::length_error);
    EXPECT_NO_THROW(dyn.prepare(15));
}

// Regression: capacity() reported the whole ring even when max_size capped the
// adapter far below it, contradicting max_size() and prepare(). Asio derives its
// read sizes from capacity(), so it must never promise room prepare() refuses.
TEST(DynamicBufferTests, CapacityNeverExceedsMaxSize) {
    stream_buffer sb(1024, 16);

    dynamic_buffer capped(sb, 16);
    EXPECT_EQ(capped.capacity(), 16u);
    EXPECT_LE(capped.capacity(), capped.max_size());
    EXPECT_THROW(capped.prepare(capped.capacity() + 1), std::length_error);

    dynamic_buffer uncapped(sb);
    EXPECT_EQ(uncapped.capacity(), sb.capacity());
}

// Regression: an empty shared_ptr was dereferenced in a noexcept constructor,
// terminating instead of reporting the error.
TEST(DynamicBufferTests, NullSharedPtrThrowsInvalidArgument) {
    EXPECT_THROW(dynamic_buffer<stream_buffer>(std::shared_ptr<stream_buffer>{}),
                 std::invalid_argument);
    EXPECT_THROW(dynamic_buffer<stream_buffer>(std::shared_ptr<stream_buffer>{}, 16),
                 std::invalid_argument);
}

// The reference constructor stores no shared_ptr (no control block, no atomics on the
// copy path), but buffer_ptr() must still hand back a non-owning handle to the backend.
TEST(DynamicBufferTests, ReferenceConstructedAdapterDoesNotOwnBuffer) {
    stream_buffer sb(1024, 16);
    dynamic_buffer dyn(sb);
    EXPECT_FALSE(dyn.owns_buffer());

    {
        auto ptr = dyn.buffer_ptr();
        EXPECT_EQ(ptr.get(), &sb);
        EXPECT_EQ(ptr.use_count(), 1);   // null deleter, not shared with the adapter
    }

    // the handle going out of scope must not have destroyed the caller-owned backend
    const std::string msg = "still alive";
    net::buffer_copy(dyn.prepare(msg.size()), net::buffer(msg));
    dyn.commit(msg.size());
    auto record = dyn.consume(msg.size());
    ASSERT_TRUE(static_cast<bool>(record));
    EXPECT_EQ(record.length, msg.size());

    // copies of a non-owning adapter stay non-owning
    dynamic_buffer copy = dyn;
    EXPECT_FALSE(copy.owns_buffer());
    EXPECT_EQ(&copy.buffer(), &sb);
}

TEST(DynamicBufferTests, SharedPtrConstructedAdapterOwnsBuffer) {
    stream_buffer_multiplexer mux(64);
    auto pb = mux.add_producer(0, 1024, 16);

    dynamic_buffer dyn(pb);
    EXPECT_TRUE(dyn.owns_buffer());

    dynamic_buffer copy = dyn;   // asio copies DynamicBuffer_v1 objects by value
    EXPECT_TRUE(copy.owns_buffer());
    EXPECT_EQ(&copy.buffer(), pb.get());
}
