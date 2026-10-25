# mini-socket-server

Two small TCP echo servers written while learning sockets, as preparation for
`ft_irc`: one blocking, one non-blocking.

> Not part of the 42 curriculum. Built by **Arda Aycan** as a learning exercise.

Both listen on **port 8080** and send back whatever they receive. The difference
is how they wait for clients.

| | Blocking | Non-blocking |
| --- | --- | --- |
| Waiting | `accept()` blocks until a client arrives | `poll()` waits on every socket at once |
| Clients | one at a time, start to finish | many at once |
| Socket mode | default blocking | `fcntl(O_NONBLOCK)` on the listening and client sockets |
| Writes | `write()` is assumed to send everything | partial writes are queued per client and flushed later |

## What each one shows

**`ft_blocking_echo_server/`**

A straight line through the socket API: `socket`, `setsockopt(SO_REUSEADDR)`,
`bind`, `listen`, then `accept` → `read` → `write` → `close` in a loop. Simple
and honest, but a client that connects and says nothing stops everyone else.

**`ft_non_blocking_echo_server/`**

The same server with a `std::vector<pollfd>` and a `std::map<int, std::string>`
of pending writes:

- `poll()` reports which sockets are readable or writable.
- New connections are added to the vector; closed ones are removed after the
  loop, never during it.
- Reads are appended to a per-client buffer and echoed.
- If `write()` accepts only part of a response, the rest stays in the pending
  map until the socket is writable again.

That last part is the real lesson: on a non-blocking socket, `write()` returning
a short count is normal, not an error.

## Build and run

```bash
g++ -Wall -Wextra -Werror ft_blocking_echo_server/blocking_echo_server.cpp \
    -o blocking_echo_server
./blocking_echo_server
```

```bash
g++ -Wall -Wextra -Werror ft_non_blocking_echo_server/non_blocking_echo_server.cpp \
    -o non_blocking_echo_server
./non_blocking_echo_server
```

From another terminal:

```bash
nc localhost 8080
hello
hello
```

With the non-blocking server, open several `nc` sessions at once and type in any
of them — they all get their echo.

## Layout

```
ft_blocking_echo_server/blocking_echo_server.cpp
ft_non_blocking_echo_server/non_blocking_echo_server.cpp
```

## Notes

- Linux/POSIX sockets; no external libraries.
- The code is C++ (the standard library is used for the client list and
  buffers) but the socket calls are plain C.
- There is no graceful shutdown signal handler: Ctrl+C ends the process.

## Author

**Arda Aycan** ([@EpicYamo](https://github.com/EpicYamo)).
