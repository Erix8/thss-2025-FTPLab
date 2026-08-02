# Server Implementation (`MyServer/`)

## Entry Point (`main.c`)

1. Parse and validate the command-line configuration (`-port`, `-root`).
2. Create the listening TCP socket on the configured port.
3. Run the multi-client connection manager (event loop).
4. Close the listening socket on shutdown.

## Configuration (`config/`)

- **`config_init()`** parses `-port n` and `-root /path`. Defaults are `port=21` and `root=/tmp`.
  - The root path is normalized (`realpath`) when possible; relative paths are resolved against the current working directory first.
- **`config_validate()`** checks that the port is within `[1, 65535]` and that the root directory exists, is a directory, and is readable and searchable (`R_OK | X_OK`).
- A default `max_conn = 20` limits concurrent clients.

## Connection Management (`conn/`) — Multi-client via `select()`

- **Dynamic connection list (`ClientConnList`)**: A growable/shrinkable array of `ClientConn` entries. Capacity doubles when full and halves when `used < capacity/2` (minimum 4). Free slots are reused before growing.
- **`ClientConn`** tracks per-client state:
  - `ctrl_fd`, `data_fd`, `pasv_listen_fd` — control / data / passive-listening sockets
  - `auth_state` — unauthenticated vs. authenticated
  - `pending_user_anon` — whether `USER anonymous` has been received and `PASS` is expected
  - `current_dir` / `root_dir` — absolute current working directory and FTP root
  - `data_mode` — `NONE` / `PORT` / `PASV`
  - `data_host` / `data_port` — target endpoint for active (PORT) mode
  - `xfer_in_progress` — set while a RETR/STOR/LIST transfer is ongoing
- **`conn_manager_run()`** is the main `select()`-based event loop:
  1. Rebuilds the read `fd_set` each iteration from the listening socket + all active control sockets.
  2. New connections are accepted, initialized, added to the list, and greeted with `220 Anonymous FTP server ready.`
  3. If `max_conn` is reached, the connection is rejected with `421 Too many connections`.
  4. Ready control sockets are serviced by `handle_client_cmd()`, which reads a line (`socket_recv`, CRLF stripped), ignores commands while a data transfer is in progress for that client, splits it into verb + args, handles `QUIT` (replies `221 Goodbye.` and removes the connection), and dispatches everything else to `cmd_process()`.
  5. A `SIGINT` handler cleans up all connections and exits gracefully.
  6. A 1-second `select()` timeout prevents permanent blocking.

## Network Layer (`net/`)

- `socket_create_listen()` — create, bind (`SO_REUSEADDR`), and listen on the control port.
- `socket_accept()` — accept a control connection, returning the client IP/port.
- `socket_send()` — send a response line, automatically appending `\r\n` (per FTP spec).
- `socket_recv()` — receive a command line, NUL-terminate it, and strip the trailing `\r\n`.

## Command Handling (`cmd/`) — FTP Commands

Authentication state machine:

1. **Not logged in, no pending `USER`** → only `USER` is accepted; anything else gets `530 Please login with USER anonymous.`
2. **`USER anonymous` received** → only `PASS` is accepted (further `USER` gets `331 User accepted, send PASS (email address).`)
3. **Authenticated** → all other commands are available.

Implemented commands:

| Command | Behavior |
| --- | --- |
| `USER anonymous` | Validates the username is `anonymous`; replies `331 Please specify the password.` If already logged in, replies `503 Already logged in.` Other usernames get `530 Only anonymous login supported.` |
| `PASS <email>` | If a non-empty password is provided, logs the client in with `230 Login successful.` Errors: `503` (no preceding USER), `501` (empty password). |
| `PORT h1,h2,h3,h4,p1,p2` | Stores the client's active-mode endpoint; closes any previous data/passive sockets; replies `200 PORT command successful.` (RFC semantics: a new PORT drops any existing data connection and stops passive listening.) |
| `PASV` | Opens a new listening socket on a random port in `[20000, 65535]`; replies `227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)`, where the IP is taken from `getsockname()` on the control socket so it reflects the address the client actually talks to. |
| `RETR <filename>` | Verifies auth (`530`), that PORT/PASV was issued (`425 Use PORT or PASV first.`), and that no transfer is running (`450`). Spawns a detached transfer thread: replies `150`, establishes the data connection, streams the file, and replies `226 Transfer complete.` (or `425`/`451` on failure). |
| `STOR <filename>` | Same pattern as RETR, receiving the file and writing it with mode `0644` (`O_CREAT|O_WRONLY|O_TRUNC`). Failure replies `550 Failed to create or write file.` |
| `CWD <path>` | Resolves the path (absolute paths are interpreted relative to the FTP root; relative paths relative to the current directory), verifies it stays inside the root, checks it is a real directory, and updates `current_dir`; replies `250 Directory successfully changed.` |
| `PWD` | Replies `257 "<display-path>"`, where the display path is the current directory expressed relative to the FTP root (`/` for the root). |
| `MKD <dirname>` | Creates a directory (mode `0755`) after verifying the parent exists and is inside the root; replies `257 "<display-path>"` on success. |
| `RMD <dirname>` | Removes an **empty** directory inside the root (`rmdir`), refusing to remove the root itself; replies `250 Directory removed.` |
| `LIST` | Streams the current directory listing over the data connection (see Data Transfer). Replies `150 … 226`. |
| `SYST` | Replies `215 UNIX Type: L8`. |
| `TYPE I` | Replies `200 Type set to I.` Any other parameter → `504 Command not implemented for that parameter.` |
| `QUIT` | Handled in the connection loop: replies `221 Goodbye.` and closes the connection. |
| (unknown) | Replies `502 Command not implemented.` |

## Path Security

Path traversal protection is enforced in two complementary ways (`utils/`):

- `utils_join_path()` — joins root/cwd with the requested path, normalizing away `.` and `..`, and **returns NULL if `..` would escape the base** (prevents `../` traversal).
- `utils_check_path()` — a final absolute check that the resolved target string is lexically inside `root` (prefix match + boundary separator check).

This covers `RETR`, `STOR`, `CWD`, `MKD`, and `RMD` — requests outside the root are rejected (e.g., `RETR ../personal.data` → error).

## Data Transfer (`transfer/`) — Threaded, Non-blocking Transfers

- **`transfer_init_data_conn()`** establishes the data connection according to `data_mode`:
  - **PORT (active):** the server `connect()`s to the client's advertised IP:port.
  - **PASV (passive):** the server `accept()`s on the passive listening socket with a 5-second `select()` timeout.
- **`transfer_send_file()`** streams a file to the data connection using a 16 KiB buffer, handling partial writes.
- **`transfer_recv_file()`** receives a file from the data connection the same way.
- **`transfer_send_list()`** forks `/bin/ls -lA -- <current_dir>` in a child process, pipes the output, strips the `total XXX` first line, converts `\n` to `\r\n` (RFC-style line endings) and streams it to the data connection. Using `/bin/ls` guarantees a familiar listing format, and the parent process keeps `CRLF` conversion in the main server process.
- **`transfer_close_data_conn()`** closes data/passive sockets and resets `data_mode` to `NONE`, so a new `PORT`/`PASV` is required before the next transfer (one-time-use data connections per RFC).
- **Non-blocking transfers (optional feature):** each `RETR`/`STOR`/`LIST` runs in a **detached pthread** (`xfer_thread`). While a transfer is in progress, `handle_client_cmd()` ignores further control commands from that client, but other clients (and even the same client's future commands after completion) remain fully serviced by the `select()` loop.

## Command / Response Formatting

- Commands are split by `utils_split_cmd()`: verb is uppercased, and a single optional space-delimited argument is passed to the handler.
- All response lines are sent with `\r\n` termination, reply codes and formats follow RFC 959.