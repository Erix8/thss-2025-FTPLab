# Assignment Requirements

## 1. FTP Server (40 points, mandatory)

The server must:

- Serve files from a designated root directory to clients making requests on a designated TCP port.
- Handle the following FTP commands:
  - **Authentication:** `USER`, `PASS`
  - **Connection mode:** `PORT`, `PASV`
  - **File transfer:** `RETR`, `STOR`
  - **Directory operations:** `CWD`, `PWD`, `MKD`, `RMD`
  - **System control:** `SYST`, `TYPE`, `QUIT`
  - The client is also required to support `LIST`, and therefore the server must serve directory listings over the data connection as well.
- Use the **Berkeley Socket API** and be written in **C**, compiled with `gcc`/`clang`, and built with **GNU Make**.
- Assume **binary mode** only for data transfers (i.e., `TYPE A` may be ignored).
- Suppress all standard output / standard error debug or trace information.
- Handle invalid input reasonably and return defensible error codes, e.g., reject requests for files outside the root directory (security: disallow `RETR ../personal.data`).
- Support integral large-file transmission of approximately **1 GB**.
- Handle multiple clients simultaneously. A single-threaded server using `select()` is adequate, or multi-threading with pthreads / multi-processing with `fork`.

**Command-line arguments (in any order, defaults shown):**

| Option | Meaning                                    | Default      |
| ------ | ------------------------------------------ | ------------ |
| `-port n` | TCP port the server binds and listens on | `21`         |
| `-root /path/to/file/area` | Root directory for all requests | `/tmp` |

## 2. FTP Client (30 points, mandatory)

The client must:

- Use `USER`, `PASS`, `PORT`, `PASV`, `RETR`, `STOR`, `CWD`, `PWD`, `MKD`, `RMD`, `LIST`, `SYST`, `TYPE`, `QUIT` to log in to a (possibly commercial) FTP server, download/upload files, and manipulate directories.
- Run on GNU/Linux, written in **C or Python** — **no FTP libraries allowed** — and must include an executable in the submission.
- Use binary mode for transfers.
- Support integral large-file transmission of approximately **1 GB**.

**Command-line arguments (in any order, defaults shown):**

| Option | Meaning                                     | Default       |
| ------ | ------------------------------------------- | ------------- |
| `-ip IPaddress` | IP address of the FTP server (`xxx.xxx.xxx.xxx`) | `127.0.0.1` |
| `-port n` | TCP port the server listens on | `21` |

## 3. Optional Features (+5 points each, +10 max)

- Support connections from multiple clients simultaneously (implemented via `select()`).
- Resume transmission after connection termination.
- File transmission without blocking the server (implemented via per-client transfer threads).
- User-friendly GUI (the client must also accept command-line input).

## 4. Grading Notes

- Compiler warnings reduce credit; a core dump during testing loses substantial credit.
- Poor design, documentation, or code structure reduces the grade; **putting all code in one module is an egregious design failure**.
- Public test cases are provided; the implemented features must pass them.