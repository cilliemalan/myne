# myne
For now, a working static http server written in c++

# Info
Uses non-blocking IO with epoll directly. Gets pretty good performance (on par with nginx).
Building the project requires a complicated setup at the moment, so for now you're on your own.

# Prerequisites
You will need:
- nghttp2 (libnghttp2-dev)
- OpenSSL (libssl-dev)