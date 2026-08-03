# Socket Programming (FTP Client & Server)

A miniature **FTP server** and **FTP client** written from scratch in **C** using the **Berkeley Socket API**, running on GNU/Linux over TCP.

- **Server** (`MyServer/`): multi-client FTP server supporting `USER`, `PASS`, `PORT`, `PASV`, `RETR`, `STOR`, `CWD`, `PWD`, `MKD`, `RMD`, `LIST`, `SYST`, `TYPE`, `QUIT`.
- **Client** (`MyClient/`): interactive command-line FTP client using the same command set to log in, navigate directories, and upload/download files.

## Documentation

| Document | Contents |
| --- | --- |
| [doc/requirements.md](doc/requirements.md) | Assignment requirements (commands, CLI options, scoring, optional features) |
| [doc/server.md](doc/server.md) | Server implementation details (event loop, commands, data transfer, path security) |
| [doc/client.md](doc/client.md) | Client implementation details (command dispatch, I/O, UI) |
| [doc/testing.md](doc/testing.md) | How to run the local auto-grading scripts and manually verify the private scoring points |
| [doc/design.md](doc/design.md) | Key design decisions and notable features |

## Project Structure

```
.
├── README.md
├── doc/                           # Detailed documentation
│   ├── requirements.md
│   ├── server.md
│   ├── client.md
│   ├── testing.md
│   └── design.md
│
├── MyClient/                      # FTP client (C)
│   ├── main.c                     # Client entry point
│   ├── Makefile                   # GNU Make recipe -> produces `client`
│   ├── autograde_client.py        # Local auto-test script for the client
│   ├── std_server.py              # Standard FTP server helper (pyftpdlib) for testing
│   ├── requirements.txt           # pyftpdlib dependency for std_server.py
│   ├── cmd/                       # Command dispatch (PASV/PORT/RETR/STOR/LIST/…)
│   ├── net/                       # Control connection, send/recv, PORT listener
│   ├── transfer/                  # File upload/download, directory listing
│   ├── ui/                        # Read user input, print server responses
│   └── utils/                     # Command splitting, PORT arg parsing, -ip/-port parsing
│
└── MyServer/                      # FTP server (C)
    ├── main.c                     # Server entry point
    ├── Makefile                   # GNU Make recipe -> produces `server`
    ├── autograde_server.py        # Local auto-test script for the server
    ├── config/                    # Parse/validate -port and -root options
    ├── conn/                      # Dynamic connection list + select() event loop
    ├── net/                       # Listen/accept/send/recv/close helpers
    ├── cmd/                       # FTP command handlers (USER…QUIT)
    ├── transfer/                  # Data connection + file/dir transfer
    └── utils/                     # Path joining/checking, command splitting, PORT parsing
```

## Build & Run

### Server

```bash
cd MyServer
make                          # produces ./server (compiled with -Wall -Wextra, no warnings)
./server                      # default: port 21, root /tmp (may need sudo for port 21)
./server -port 2121 -root /tmp/ftproot
make clean
```

### Client

```bash
cd MyClient
make                          # produces ./client
./client                      # default: 127.0.0.1:21
./client -ip 127.0.0.1 -port 2121
make clean
```

### Example session

```
$ ./client -ip 127.0.0.1 -port 2121
220 Anonymous FTP server ready.
USER anonymous
331 Please specify the password.
PASS guest@example.com
230 Login successful.
PASV
227 Entering Passive Mode (127,0,0,1,199,42)
LIST
150 Opening data connection.
drwxr-xr-x  2 user user 4096 … .
drwxr-xr-x  2 user user 4096 … ..
-rw-r--r--  1 user user 1234 … hello.txt
226 Transfer complete.
RETR hello.txt
150 Opening data connection.
226 Transfer complete.
QUIT
221 Goodbye.
```

## Testing

Two kinds of testing are covered in [doc/testing.md](doc/testing.md): running the provided auto-grading scripts that mirror the public test cases, and manually verifying the private scoring points that the scripts do not cover.

```bash
# Server public autograde (needs sudo for default port 21)
cd MyServer
python3 autograde_server.py

# Client public autograde (needs pyftpdlib)
pip install -r MyClient/requirements.txt
cd ../MyClient
python3 autograde_client.py
```

The manual private-test checklist (both data modes, directory operations, path-traversal rejection, invalid-input error codes, ~1 GB transfers, CLI defaults, standard-server compatibility, etc.) is in the **Private Test Cases** section of [doc/testing.md](doc/testing.md).

## Highlights

- **Multi-client** via a `select()`-based event loop (no busy-waiting).
- **Non-blocking transfers**: each `RETR`/`STOR`/`LIST` runs in a detached pthread.
- **Path traversal protection** (`..` escapes rejected) on all file/directory commands.
- **PORT and PASV** data connection modes, with one-time-use data connections per RFC 959.
- Streams large files (~1 GB) in fixed-size chunks; all responses use RFC-style `\r\n`.
- Clean standard output/error (all debug prints suppressed).