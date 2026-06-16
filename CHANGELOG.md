# Changelog

## v1.0.0 - 2026-06-16

### Added

- `slick::dynamic_buffer<BufferT>` — Boost.Asio `DynamicBuffer_v1` adapter, drop-in
  replacement for `boost::beast::flat_buffer`. `consume(n)` publishes the consumed
  bytes as one message record (zero-copy fan-out) via the backend.
- `slick::buffer_backend` C++20 concept defining the required backend interface
  (`prepare`, `commit`, `consume`, `discard`, `data`, `size`, `capacity`).
- Two construction modes: `dynamic_buffer(std::shared_ptr<BufferT>)` for shared
  ownership and `dynamic_buffer(BufferT&)` for non-owning reference (null deleter).
- `buffer()` accessor returning `BufferT&`; `buffer_ptr()` returning
  `std::shared_ptr<BufferT>` for shared lifetime management.
- Supported backends out of the box:
  - `slick::stream_buffer` — single-producer ring buffer.
  - `slick::stream_buffer_multiplexer::producer_buffer` — per-producer ring that
    fans records into the shared MPMC queue.
- Only requires Boost.Asio as a library dependency; backend is caller-supplied.
- GoogleTest suites covering buffer copy, max-size clamping, TCP loopback zero-copy
  read, Asio composed ops, and both slick backends; CI for Windows/Linux/macOS.
