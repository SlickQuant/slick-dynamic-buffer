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

// Consumer of the *installed* package. It uses a self-contained backend so the test
// exercises packaging (header install, C++20 propagation, Boost resolution through the
// generated config) without pulling in slick-stream-buffer.

#include <slick/dynamic_buffer.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {

class stub_backend {
public:
    std::pair<uint8_t*, std::size_t> prepare(std::size_t n) {
        if (n > storage_.size() - size_) {
            throw std::length_error("stub_backend: prepare too large");
        }
        return { storage_.data() + size_, n };
    }
    void commit(std::size_t n) noexcept { size_ += n; }
    std::size_t consume(std::size_t n) noexcept {
        std::memmove(storage_.data(), storage_.data() + n, size_ - n);
        size_ -= n;
        return n;
    }
    void discard() noexcept { size_ = 0; }
    const uint8_t* data() const noexcept { return storage_.data(); }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return storage_.size(); }

private:
    std::array<uint8_t, 256> storage_{};
    std::size_t size_ = 0;
};

static_assert(slick::buffer_backend<stub_backend>,
              "stub_backend must satisfy slick::buffer_backend");

int failures = 0;

void check(bool ok, const char* what, int line) {
    if (!ok) {
        std::fprintf(stderr, "consumer.cpp:%d: FAILED: %s\n", line, what);
        ++failures;
    }
}

}  // namespace

#define CHECK(cond) check((cond), #cond, __LINE__)

int main() {
    stub_backend backend;
    slick::dynamic_buffer buf(backend, 16);   // CTAD, capped well below the backend

    CHECK(buf.max_size() == 16);
    CHECK(buf.capacity() == 16);              // not the 256-byte backend capacity
    CHECK(!buf.owns_buffer());

    auto region = buf.prepare(8);
    CHECK(region.size() == 8);
    std::memcpy(region.data(), "abcdefgh", 8);
    buf.commit(8);
    CHECK(buf.size() == 8);
    CHECK(std::memcmp(buf.data().data(), "abcdefgh", 8) == 0);

    bool threw = false;
    try {
        buf.prepare((std::numeric_limits<std::size_t>::max)());
    } catch (const std::length_error&) {
        threw = true;
    }
    CHECK(threw);                             // must not wrap past the cap

    CHECK(buf.consume(8) == 8);
    CHECK(buf.size() == 0);

    threw = false;
    try {
        slick::dynamic_buffer<stub_backend> null_buf(std::shared_ptr<stub_backend>{});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);

    if (failures == 0) {
        std::printf("installed slick-dynamic-buffer consumer: OK\n");
    }
    return failures == 0 ? 0 : 1;
}
