# Changelog

## v1.0.0 - 2026-06-12

Initial release.

- `slick::dynamic_buffer` — Boost.Asio `DynamicBuffer_v1` adapter, drop-in
  replacement for `boost::beast::flat_buffer`; `consume(n)` publishes the consumed
  bytes to consumers as one message record (zero-copy fan-out).
- The API uses the `slick::stream_buffer` spelling of the core buffer, so
  slick-stream-buffer >= v1.0.4 is required.
- GoogleTest suites for the asio adapter (including a real TCP loopback test); CI for Windows/Linux/macOS.
