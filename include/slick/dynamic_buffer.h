/********************************************************************************
 * Copyright (c) 2026 Slick Quant LLC
 * All rights reserved
 *
 * This file is part of the slick-dynamic-buffer. Redistribution and use in source
 * and binary forms, with or without modification, are permitted exclusively
 * under the terms of the MIT license which is available at
 * https://github.com/SlickQuant/slick-dynamic-buffer/blob/main/LICENSE
 *
 ********************************************************************************/

#pragma once

#include <slick/stream_buffer.h>
#include <boost/asio/buffer.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace slick {

/**
 * @brief Boost.Asio DynamicBuffer_v1 adapter over slick::stream_buffer.
 *
 * A drop-in replacement for beast::flat_buffer: pass it to boost::asio/boost::beast read
 * operations so received network bytes are written directly into the stream_buffer ring -
 * no copy is needed to hand the data to other threads or processes. When the application has
 * a complete package in the readable area, calling consume(n) publishes those n bytes to
 * consumers as one message record (it does not discard them).
 *
 * This is a cheap copyable handle: asio composed operations copy DynamicBuffer_v1 objects by
 * value, so the adapter holds a pointer to the stream_buffer, which the application owns.
 * In-process consumers call read() on the stream_buffer directly (see stream_buffer());
 * other processes open it by shared memory name.
 *
 * Note: each consume(n) call publishes exactly one record. If a protocol layer consumes
 * incrementally (e.g. the beast HTTP parser), records correspond to those increments; call
 * consume() yourself on message boundaries when you need strict package framing.
 */
class dynamic_buffer {
public:
    /// The type used to represent the readable bytes as a single contiguous buffer
    using const_buffers_type = boost::asio::const_buffer;
    /// The type used to represent the writable bytes as a single contiguous buffer
    using mutable_buffers_type = boost::asio::mutable_buffer;

    /**
     * @brief Wrap a slick::stream_buffer.
     * @param buffer The stream buffer; must outlive this adapter and all copies of it.
     * @param max_size Optional cap on size() + prepared bytes; clamped to buffer.capacity().
     *                 beast/asio read operations use max_size() to limit how much they read.
     */
    explicit dynamic_buffer(
        slick::stream_buffer& buffer,
        std::size_t max_size = (std::numeric_limits<std::size_t>::max)()) noexcept
        : buffer_(&buffer)
        , max_size_(max_size < buffer.capacity() ? max_size : static_cast<std::size_t>(buffer.capacity()))
    {
    }

    /// The number of readable (committed but not consumed) bytes
    std::size_t size() const noexcept {
        const std::size_t n = buffer_->size();
        return n < max_size_ ? n : max_size_;
    }

    /// The maximum sum of readable and writable bytes
    std::size_t max_size() const noexcept { return max_size_; }

    /// The maximum sum of readable and writable bytes that can be held without relocation
    std::size_t capacity() const noexcept { return static_cast<std::size_t>(buffer_->capacity()); }

    /// The readable bytes as a single contiguous buffer
    const_buffers_type data() const noexcept {
        return { buffer_->data(), size() };
    }

    /**
     * @brief Get a writable region of n bytes at the end of the readable area.
     * @throws std::length_error if size() + n exceeds max_size().
     */
    mutable_buffers_type prepare(std::size_t n) {
        if (buffer_->size() + n > max_size_) {
            throw std::length_error("dynamic_buffer too long");
        }
        auto [ptr, sz] = buffer_->prepare(n);
        return { ptr, sz };
    }

    /// Move n bytes from the writable area to the readable area
    void commit(std::size_t n) noexcept { buffer_->commit(n); }

    /// Publish the first n readable bytes to consumers as one message record.
    /// Returns the record as consumers will see it (asio/beast callers may ignore it).
    slick::stream_buffer::published_record consume(std::size_t n) noexcept { return buffer_->consume(n); }

    /// Discard the readable bytes and any prepared region without publishing them,
    /// matching beast::flat_buffer::clear(). Use after a connection drops mid-message
    /// so the partial bytes are not prepended to the next connection's data. This
    /// does not create a new record; older published records still follow the normal
    /// lossy overwrite semantics.
    void clear() noexcept { buffer_->discard(); }

    /// Access the underlying stream_buffer (e.g. for in-process consumers).
    /// The type must be spelled slick::stream_buffer inside this class because
    /// this member function shadows the unqualified type name.
    slick::stream_buffer& stream_buffer() noexcept { return *buffer_; }
    const slick::stream_buffer& stream_buffer() const noexcept { return *buffer_; }

private:
    slick::stream_buffer* buffer_;
    std::size_t max_size_;
};

}  // namespace slick
