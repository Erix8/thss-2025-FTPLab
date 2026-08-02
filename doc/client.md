# Client Implementation (`MyClient/`)

## Entry Point (`main.c`)

1. Initialize the client state.
2. Parse `-ip` and `-port` (defaults `127.0.0.1:21`).
3. Connect the control socket, read (and print) the server's `220` greeting.
4. Enter an interactive loop: read a line, dispatch it, and exit when the user enters `QUIT` or EOF.
5. Close the control connection.

## Command Dispatch (`cmd/`)

The client is a **line-based interactive CLI**. `client_handle_input()` splits the input into verb + args and handles the data-connection commands specially:

- **`PASV`** — sends `PASV`, parses the `227 (h1,h2,h3,h4,p1,p2)` response, and proactively connects the data socket to the server's advertised endpoint. The data connection is stored in `client->data_fd`.
- **`PORT`** — parses the `h1,h2,h3,h4,p1,p2` argument, creates a **local listening socket** on the given IP:port (`client_listen_port()`), sends `PORT …`, and awaits the server's inbound data connection (accepted later by `RETR`/`STOR`/`LIST`).
- **`RETR <file>`** — sends `RETR`, prints the `150` mark, establishes the data connection (PASV: already connected; PORT: `accept()` on the local listener), receives the file to the local basename of the remote path, prints the final `226`, closes the data connection, and resets `data_mode` to `NONE`.
- **`STOR <local file>`** — sends `STOR`, prints the `150` mark, establishes the data connection, uploads the file, **closes the data connection first** (the server sends its final `226` only after the client closes the data connection), and prints the final response.
- **`LIST`** — sends `LIST`, prints the `150` mark, establishes the data connection, receives the directory listing, prints the final `226`, and resets data mode.
- **All other commands** (e.g., `USER`, `PASS`, `CWD`, `PWD`, `MKD`, `RMD`, `SYST`, `TYPE`, `QUIT`) are forwarded verbatim as generic commands (`client_handle_generic()`), and their responses are printed.

## Network Layer (`net/`)

- `client_connect()` — creates a TCP socket and connects to the server IP:port (`inet_pton`).
- `client_send_cmd()` — sends a command line, appending `\r\n` automatically.
- `client_recv_resp()` — receives a server response line and returns the leading 3-digit code (e.g., `220`, `550`) while also providing the full text for display.
- `client_listen_port()` — creates a local listening socket on a specified IP:port with `SO_REUSEADDR` (used by PORT mode).

## Data Transfer (`transfer/`)

- **64 KiB buffer** for efficient large-file transfers.
- `transfer_recv_file()` — reads from the data connection, writes to the local file, **retries on `EINTR`**, and handles partial writes.
- `transfer_send_file()` — reads the local file, sends over the data connection, retries on `EINTR`, handles partial sends.
- `transfer_recv_list()` — receives the directory listing; optionally normalizes `\r\n` → `\n` (and handles a trailing lone `\r`), printing to stdout or writing to a file.

## UI (`ui/`)

- `ui_read_input()` — reads one line of user input with `fgets` and strips the trailing newline.
- `ui_print_msg()` — prints a server response cleanly (strips trailing `\r`/`\n`, one line at a time, flushed).

## Command-line Parsing (`utils/`)

- `ui_parse_args()` — accepts `-ip IPaddress` and `-port n` in any order, with defaults `127.0.0.1` and `21`.