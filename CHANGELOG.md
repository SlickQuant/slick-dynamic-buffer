# Changelog

## v1.0.2 - 2026-09-06

### Fixed

- `prepare(n)` no longer lets a huge `n` wrap past the `max_size()` check
  (`size() + n` overflowed); the remaining room is now computed by subtraction.
- `capacity()` no longer reports the full backend ring on an adapter capped by
  `max_size`. It is clamped to `max_size()`, so asio cannot size a read larger than
  `prepare()` accepts.
- The `shared_ptr` constructor validates its argument and throws `std::invalid_argument`
  for an empty pointer instead of dereferencing null in a `noexcept` function. It is
  therefore no longer `noexcept`; the reference constructor now is.
- The exported CMake target declares `target_compile_features(... cxx_std_20)`, so
  consumers compile the header as C++20 even when their project asks for an older
  standard (`CMAKE_CXX_STANDARD` in this project never applied to consumers).
- Boost is no longer effectively vcpkg-only: `Boost::asio` (modular / vcpkg) is tried
  first, then the header-only `Boost::headers` target, then the `FindBoost` module.
- The installed package is no longer tied to the Boost provider of the machine that built
  it. The exported target names no Boost target at all; the package config runs the same
  resolution on the consumer machine and attaches whatever it finds there, so a package
  built with vcpkg (`Boost::asio`) works for a consumer that only has `Boost::headers`.
  The shared resolution lives in `cmake/slick_find_boost_asio.cmake`, installed next to
  the package config.

### Changed

- The reference (non-owning) constructor stores a raw pointer instead of wrapping the
  backend in a null-deleter `shared_ptr`, removing a control-block allocation and the
  atomic reference-count traffic from every copy asio makes of the adapter.
  `buffer_ptr()` still returns a null-deleter handle for such adapters, built on demand.
- Added `owns_buffer()`, telling the two construction modes apart.
- CMake minimum raised to 3.21 (already required in practice by `PROJECT_IS_TOP_LEVEL`).

### Added

- Regression tests for the overflow in `prepare()`, the capped `capacity()`, the empty
  `shared_ptr`, and the ownership/`buffer_ptr()` behaviour of both constructors.
- `BUILD_SLICK_DYNAMIC_BUFFER_INSTALL_TEST` (on by default for top-level builds):
  installs the project into a scratch prefix and builds a C++17-configured consumer
  against it, catching regressions in the installed package config.

## v1.0.1 - 2026-06-17

- Renamed canonical header from `slick/dynamic_buffer.h` to `slick/dynamic_buffer.hpp`. The old .h path is kept as a backward-compatibility shim that re-exports the new header and emits a compiler warning directing users to update their includes.

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
