# Design Decisions & Notable Features

1. **Modular architecture** — the server and client are each split into dedicated modules (`net`, `cmd`, `transfer`, `config`/`ui`, `utils`, `conn`), avoiding the "all code in one module" failure mode called out in the assignment.

2. **Multi-client concurrency (optional feature #1, implemented)**
   - The server uses `select()` in a single-threaded event loop for efficient I/O multiplexing, satisfying the multi-client requirement without busy-waiting.

3. **Non-blocking file transmission (optional feature #3, implemented)**
   - Each `RETR`/`STOR`/`LIST` runs in a detached pthread. While a transfer is active, the affected client's control commands are ignored, but the `select()` loop continues serving all other clients and new connections — transfers never block the server.

4. **Security: path traversal protection**
   - `utils_join_path()` normalizes `..` and rejects escapes; `utils_check_path()` enforces the root boundary as a second check. This covers `RETR`, `STOR`, `CWD`, `MKD`, and `RMD` — requests outside the root are rejected (e.g., `RETR ../personal.data` → error).

5. **RFC-consistent behavior details**
   - All responses use `\r\n`.
   - `PORT` and `PASV` data connections are **one-time use**; the mode resets to `NONE` after each transfer.
   - A new `PORT`/`PASV` drops any previous data/passive sockets.
   - `PASV` advertises the address obtained via `getsockname()` on the control socket, so NAT/proxy-bound clients receive a routable endpoint.
   - 5-second timeout on passive mode `accept()`, avoiding indefinite hangs.
   - `LIST` output is produced by `/bin/ls -lA`, the familiar UNIX format, with the `total` line removed and line endings converted to `\r\n`.

6. **Robust I/O**
   - Data transfers handle partial reads/writes, `EINTR` retries (client side), and use reasonably large buffers (64 KiB client, 16 KiB server).
   - Large files (~1 GB) are streamed in fixed-size chunks rather than buffered in memory.

7. **Suppressed debug output**
   - All debug/trace `printf`s are commented out; standard out/err stays clean as required.

8. **Graceful lifecycle management**
   - Dynamic connection list shrinks when underutilized; all sockets are closed on logout and on `SIGINT`; `SIGINT` triggers a clean shutdown.