# Slick Dynamic Buffer

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)](#installation)
[![Lock-free](https://img.shields.io/badge/concurrency-lock--free-orange.svg)](#how-it-works)

`slick::dynamic_buffer` is a header-only Boost.Asio `DynamicBuffer_v1` adapter over
[slick-stream-buffer](https://github.com/SlickQuant/slick-stream-buffer), the lock-free
single-producer multi-consumer (SPMC) byte stream buffer. It is designed as a
**drop-in replacement for `boost::beast::flat_buffer`**: network bytes received by
boost::asio / boost::beast are written directly into the SlickStreamBuffer ring, and
publishing a complete message to consumer threads — or other processes via shared
memory — requires **zero copies**.

## How it works

The adapter exposes the familiar dynamic-buffer interface
(`prepare` / `commit` / `consume` / `data` / `size`), with one twist:

- `prepare(n)` returns a contiguous writable region — asio writes received bytes there
- `commit(n)` moves bytes into the readable area — the app parses them in place
- `consume(n)` does **not** discard bytes: it **publishes** them to consumers as
  **one discrete message record**

```
 network ──asio──▶ prepare/commit ──▶ [ data ring ] ──consume(n)──▶ record {offset, len}
                                                                        │
                                              consumer A (own cursor) ◀─┤  zero-copy reads
                                              consumer B (own cursor) ◀─┤  (threads or
                                              process C (shared memory)◀┘   processes)
```

Each consumer owns an independent cursor and reads whole messages zero-copy directly
from the underlying SlickStreamBuffer — the consumer side is provided by
[slick-stream-buffer](https://github.com/SlickQuant/slick-stream-buffer); see its
documentation for the full core API.

## Features

- **Boost.Asio DynamicBuffer adapter** usable with boost::beast / boost::asio read operations
- **Zero-copy fan-out** of received network data to threads and processes
- **Lock-free** single-producer / multi-consumer broadcast via slick-stream-buffer
- **Drop-in replacement** for `boost::beast::flat_buffer` (including `clear()`)
- **Cross-platform** — Windows, Linux, macOS
- Modern **C++20**, header-only

## Requirements

- C++20 compatible compiler
- [slick-stream-buffer](https://github.com/SlickQuant/slick-stream-buffer) v1.0.4+ (fetched automatically when not installed)
- Boost.Asio — only the buffer types are used

## Installation

Header-only. Add the `include` directory to your include path:

```cpp
#include <slick/dynamic_buffer.h>
```

### Using CMake FetchContent

```cmake
include(FetchContent)

set(BUILD_SLICK_DYNAMIC_BUFFER_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slick-dynamic-buffer
    GIT_REPOSITORY https://github.com/SlickQuant/slick-dynamic-buffer.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(slick-dynamic-buffer)

target_link_libraries(your_target PRIVATE slick::dynamic_buffer) # slick-stream-buffer is fetched automatically
```

## Usage

### Producer: receive with boost::asio, publish on message boundaries

```cpp
#include <slick/dynamic_buffer.h>

// 64 MB data ring, 64K message records; named -> shared memory, nullptr -> local
slick::stream_buffer stream(1ull << 26, 1u << 16, "market_data");
slick::dynamic_buffer buffer(stream);   // cheap copyable handle

for (;;) {
    std::size_t n = socket.read_some(buffer.prepare(64 * 1024));
    buffer.commit(n);

    // parse the readable area; publish every complete package
    while (std::size_t package_size = find_complete_package(buffer.data())) {
        buffer.consume(package_size);   // publishes one record - no copy
    }
}
```

The adapter satisfies asio's `DynamicBuffer_v1` requirements, so it also works with
composed operations such as `boost::asio::read(socket, buffer, ...)`,
`boost::beast::http::read(...)` and `websocket::stream::read(...)`.

### Consumers: independent cursors, zero-copy reads

Consumers read from the underlying SlickStreamBuffer directly:

```cpp
// same process:
slick::stream_buffer& stream = buffer.stream_buffer();
// another process:
slick::stream_buffer stream("market_data");

uint64_t cursor = stream.initial_reading_index();   // or 0 to replay history
for (;;) {
    auto [data, length] = stream.read(cursor);
    if (data == nullptr) continue;          // nothing new yet
    handle_package(data, length);           // points directly into the ring
}
```

See the [slick-stream-buffer](https://github.com/SlickQuant/slick-stream-buffer)
documentation for the consumer API (`read`, `read_last`, `initial_reading_index`,
`loss_count`) and ring sizing guidance.

## API Overview

```cpp
explicit dynamic_buffer(slick::stream_buffer& buffer,
                        std::size_t max_size = /* unlimited */) noexcept;
```

Wraps a `slick::stream_buffer` (the preferred spelling of `SlickStreamBuffer`, available
since slick-stream-buffer v1.0.4), which must outlive the adapter and all copies of it.
`max_size` caps `size()` plus prepared bytes (clamped to `buffer.capacity()`); beast/asio
read operations use it to limit how much they read. The adapter is a cheap copyable
handle — asio composed operations copy `DynamicBuffer_v1` objects by value.

- `mutable_buffers_type prepare(std::size_t n)` — contiguous writable region of n bytes;
  throws `std::length_error` if `size() + n > max_size()`
- `void commit(std::size_t n)` — make n prepared bytes readable
- `published_record consume(std::size_t n)` — publish the first n readable bytes as **one
  message record**; returns the record exactly as consumers will see it
  (asio/beast callers may ignore the return value)
- `void clear()` — drop the readable bytes and any prepared region **without publishing**,
  matching `beast::flat_buffer::clear()`
- `const_buffers_type data()` / `std::size_t size()` — the readable (committed, unconsumed) region
- `std::size_t max_size()` / `std::size_t capacity()` — limits, as required by `DynamicBuffer_v1`
- `slick::stream_buffer& stream_buffer()` — access the underlying buffer (e.g. for in-process consumers)

## Important Constraints

**Single producer.** All adapter (producer) methods must be called from one thread.
Consumers are lock-free and independent.

**Lossy semantics.** The producer never blocks; if it laps a slow consumer, the consumer
skips ahead and the loss is counted. Size the rings so this cannot happen in normal
operation — see [slick-stream-buffer](https://github.com/SlickQuant/slick-stream-buffer)
for details and loss detection.

**Pointer invalidation.** `prepare()` may relocate the readable region to keep it
contiguous when the ring wraps; pointers previously returned by `data()`/`prepare()`
are invalidated — the same rule as `flat_buffer` reallocation.

**Record granularity.** Every `consume(n)` call produces exactly one consumer-visible
record. If a protocol layer consumes incrementally (e.g. the beast HTTP parser),
records correspond to those increments; call `consume()` yourself on package
boundaries when you need strict framing.

**Disconnects mid-message.** If the connection drops after a partial message was
committed, call `clear()` before reconnecting so the leftover bytes are not prepended
to the new connection's data. `clear()` does not publish a record.

## Building and Testing

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Boost.Asio is required to build the tests (e.g. configure with a vcpkg toolchain file).

## License

slick-dynamic-buffer is released under the [MIT License](LICENSE).

**Made with ⚡ by [SlickQuant](https://github.com/SlickQuant)**
