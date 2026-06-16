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

#include <boost/asio/buffer.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace slick {

/**
 * @brief Named requirements (C++20 concept) for a buffer backend usable with dynamic_buffer.
 *
 * Satisfied by slick::stream_buffer and
 * slick::stream_buffer_multiplexer::producer_buffer. The backend must be
 * single-producer: all prepare/commit/consume/discard calls for one instance
 * must come from a single thread.
 */
template<typename T>
concept buffer_backend = requires(T& b, const T& cb, std::size_t n) {
    { b.prepare(n) }  -> std::same_as<std::pair<uint8_t*, std::size_t>>;
    { b.commit(n) };
    { b.consume(n) };
    { b.discard() };
    { cb.data() }     -> std::same_as<const uint8_t*>;
    { cb.size() }     -> std::convertible_to<std::size_t>;
    { cb.capacity() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Boost.Asio DynamicBuffer_v1 adapter over a slick buffer backend.
 *
 * A drop-in replacement for beast::flat_buffer: pass it to boost::asio/boost::beast
 * read operations so received network bytes are written directly into the backend ring —
 * no copy is needed to hand the data to other threads or processes. When the application
 * has a complete package in the readable area, calling consume(n) publishes those n bytes
 * to consumers as one message record (it does not discard them).
 *
 * Supported backends:
 *  - slick::stream_buffer  — SPMC ring; consumers call stream_buffer.read(cursor)
 *  - slick::stream_buffer_multiplexer::producer_buffer — fans into the shared MPMC queue
 *
 * This is a cheap copyable handle: asio composed operations copy DynamicBuffer_v1 objects
 * by value, so the adapter stores a std::shared_ptr to the backend. Two constructors are
 * provided: a shared-ownership one (pass shared_ptr<BufferT>, e.g. from add_producer())
 * and a non-owning reference one (pass BufferT&, caller must ensure the backend outlives
 * all copies).
 *
 * Note: each consume(n) call publishes exactly one record. If a protocol layer consumes
 * incrementally (e.g. the beast HTTP parser), records correspond to those increments; call
 * consume() yourself on message boundaries when you need strict package framing.
 */
template<buffer_backend BufferT>
class dynamic_buffer {
public:
    /// The type used to represent the readable bytes as a single contiguous buffer
    using const_buffers_type   = boost::asio::const_buffer;
    /// The type used to represent the writable bytes as a single contiguous buffer
    using mutable_buffers_type = boost::asio::mutable_buffer;

    /**
     * @brief Shared-ownership constructor — participates in the backend's ref count.
     * @param ptr   Shared pointer to the backend; the backend stays alive as long as
     *              this adapter (or any copy) holds a reference.
     * @param max_size Optional cap on size() + prepared bytes; clamped to backend capacity.
     */
    explicit dynamic_buffer(
        std::shared_ptr<BufferT> ptr,
        std::size_t max_size = (std::numeric_limits<std::size_t>::max)()) noexcept
        : buffer_(std::move(ptr))
        , max_size_(max_size < buffer_->capacity()
                        ? max_size
                        : static_cast<std::size_t>(buffer_->capacity()))
    {
    }

    /**
     * @brief Non-owning reference constructor — backward-compatible with local backends.
     * @param buffer The backend; must outlive this adapter and all copies of it.
     * @param max_size Optional cap on size() + prepared bytes; clamped to buffer.capacity().
     *                 beast/asio read operations use max_size() to limit how much they read.
     *
     * Internally wraps the reference in a shared_ptr with a null deleter so copies are
     * cheap, but lifetime management remains the caller's responsibility.
     */
    explicit dynamic_buffer(
        BufferT& buffer,
        std::size_t max_size = (std::numeric_limits<std::size_t>::max)())
        : dynamic_buffer(std::shared_ptr<BufferT>(&buffer, [](BufferT*){}), max_size)
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
    /// The return type is deduced from BufferT::consume() — published_record for slick backends.
    auto consume(std::size_t n) noexcept { return buffer_->consume(n); }

    /// Discard the readable bytes and any prepared region without publishing them,
    /// matching beast::flat_buffer::clear(). Use after a connection drops mid-message
    /// so the partial bytes are not prepended to the next connection's data. This
    /// does not create a new record; older published records still follow the normal
    /// lossy overwrite semantics.
    void clear() noexcept { buffer_->discard(); }

    /// Access the underlying backend (e.g. for in-process consumers calling read()).
    BufferT&       buffer() noexcept       { return *buffer_; }
    const BufferT& buffer() const noexcept { return *buffer_; }

    /// Shared-ownership handle to the backend, so it can safely outlive this adapter.
    std::shared_ptr<BufferT>       buffer_ptr() noexcept       { return buffer_; }
    std::shared_ptr<const BufferT> buffer_ptr() const noexcept { return buffer_; }

private:
    std::shared_ptr<BufferT> buffer_;
    std::size_t max_size_;
};

}  // namespace slick
