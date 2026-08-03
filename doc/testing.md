# Testing Guide

This guide covers two kinds of testing:

1. **Public Autograde** — the provided scripts in `MyServer/` and `MyClient/` that mirror the public test cases.
2. **Private Test Cases** — manual, local verification of the remaining scoring points that are **not** covered by the public tests.

Score summary: **Server = 40 pts** (public 29 + private 11), **Client = 30 pts** (public 22 + private 8).

---

## 1. Prerequisites

- **GNU/Linux** with `gcc`/`clang` and GNU `make`.
- **Root access** — the default port `21` is privileged; the server autograde script invokes `./server` via `sudo`.
- **Python 3** with the standard library `ftplib` (used by the server tests).
- **`pyftpdlib`** — required by `MyClient/std_server.py`, the standard FTP server used to test the client:

```bash
pip install -r MyClient/requirements.txt
```

## 2. Build

```bash
# server
cd MyServer
make                # must use -Wall and produce no warnings

# client
cd ../MyClient
make                # produces ./client
```

**Important:** compiler warnings reduce credit; a core dump during testing loses substantial credit. The commands below presume `./server` and `./client` have been built.

---

## 3. Public Autograde

### 3.1 Server — `MyServer/autograde_server.py` (public, 29 pts)

- Builds with `make` first, and checks that `-Wall` is present and that compilation emits no warnings.
- Starts `./server` (with optional `-port` / `-root`), then uses Python's standard `ftplib` to verify:
  - `220` greeting, `230` login, `215 UNIX Type: L8` (SYST), `200 Type set to I.` (TYPE I)
  - **`RETR` in PORT mode** — downloaded file content must match exactly (`filecmp`)
  - **`STOR` in PASV mode** — uploaded file content must match exactly
  - `221` (QUIT)
- Runs **twice**: once with default args (port 21 via `sudo`, root `/tmp`) and once with a random port and a random temporary root directory.

```bash
cd MyServer
python3 autograde_server.py
```

### 3.2 Client — `MyClient/autograde_client.py` (public, 22 pts)

- Starts `MyClient/std_server.py` (a standard `pyftpdlib` FTP server) on port `10021`.
- Starts `./client -ip 127.0.0.1 -port 10021` and feeds scripted commands via stdin, then checks the client's printed responses and the server's logged actions:
  - `220` greeting, `USER anonymous` / `PASS` login (`331` then `230`)
  - `SYST` → `215`, `TYPE I` → `200`
  - `MKD` → `257`, `CWD` → `250`, `PWD` → `257`
  - `PORT` → `200` then `RETR` (download, content verified with `filecmp`)
  - `PASV` → `227` then `RETR` (download, content verified)

```bash
cd MyClient
python3 autograde_client.py
```

These scripts are local conveniences that mirror the public test cases; they do **not** cover the private tests below.

---

## 4. Private Test Cases (Manual Verification)

The public tests cover only part of the required functionality. The following cases are not made public and should be verified locally by hand.

> Large-file tests use `dd` and `md5sum`. Keep the fixture as close to 1 GB as feasible.

### 4.1 Server private tests (11 pts)

**Setup:**

```bash
mkdir -p /tmp/ftp_root
sudo ./server -port 2121 -root /tmp/ftp_root     # any free port works
```

#### 4.1.1 Both data-connection modes for `STOR` / `RETR` (2 pts)

The public test only exercises `RETR` + PORT and `STOR` + PASV. Verify the two missing combinations:

```python
# both_modes.py
from ftplib import FTP
import os, filecmp, random

def ok(reply):
    if not reply.startswith("226"):
        raise SystemExit(f"unexpected reply: {reply}")

ftp = FTP()
ftp.connect("127.0.0.1", 2121)
ftp.login()

# STOR via PORT
ftp.set_pasv(False)
local = f"up_{random.randint(100,999)}.bin"
open(local, "wb").write(b"x" * 10000)
ok(ftp.storbinary(f"STOR {local}", open(local, "rb")))
assert filecmp.cmp(local, f"/tmp/ftp_root/{local}", shallow=False)

# RETR via PASV
ftp.set_pasv(True)
got = f"down_{random.randint(100,999)}.bin"
with open(got, "wb") as fh:
    ok(ftp.retrbinary(f"RETR {local}", fh.write))
assert filecmp.cmp(local, got, shallow=False)

os.remove(local); os.remove(got)
os.remove(f"/tmp/ftp_root/{local}")
ftp.quit()
```

Expected: both transfers return `226` and the transferred files are byte-identical.

#### 4.1.2 Directory operations: `MKD`, `CWD`, `PWD`, `RMD` (3 pts)

```python
from ftplib import FTP
ftp = FTP(); ftp.connect("127.0.0.1", 2121); ftp.login()
d = "dir_test"
print(ftp.mkd(d))     # expect 257 "/dir_test" ...
print(ftp.pwd())      # expect 257 "/"
print(ftp.cwd(d))     # expect 250
print(ftp.pwd())      # expect 257 "/dir_test"
print(ftp.rmd(d))     # expect 250
ftp.quit()
```

Expected reply prefixes: `MKD` → `257 "<path>"`, `CWD` → `250`, `PWD` → `257 "<path>"`, `RMD` → `250`. Also verify the directory really is created and then removed on disk.

#### 4.1.3 Path traversal / access outside root (2 pts)

```python
from ftplib import FTP
ftp = FTP(); ftp.connect("127.0.0.1", 2121); ftp.login()
for cmd in ["RETR ../etc/passwd", "CWD ../..", "STOR ../../evil", "RETR /etc/passwd"]:
    try:
        print(cmd, "->", ftp.sendcmd(cmd))
    except Exception as e:
        print(cmd, "-> rejected:", e)
ftp.quit()
```

Expected: every request that would escape the root directory is rejected with an error code (`550`/`501` class), and **no file is ever read from or written to outside the root**.

#### 4.1.4 Invalid input / defensible error codes (2 pts)

```python
from ftplib import FTP
ftp = FTP(); ftp.connect("127.0.0.1", 2121); ftp.login()
for cmd in ["FOO", "RETR", "TYPE A", "PASS", "CWD", "MKD", "RMD"]:
    try:
        print(cmd, "->", ftp.sendcmd(cmd))
    except Exception as e:
        print(cmd, "-> rejected:", e)
ftp.quit()
print("server still alive")
```

Expected: unknown verb → `500`/`502`; missing argument → `501`; unsupported parameter (`TYPE A`) → `504` (or another appropriate error). The server must not crash and must remain responsive for the next command.

#### 4.1.5 Large file transmission (~1 GB) (2 pts)

```bash
dd if=/dev/urandom of=/tmp/ftp_root/big.bin bs=1M count=1024
python3 - <<'EOF'
from ftplib import FTP
ftp = FTP(); ftp.connect("127.0.0.1", 2121); ftp.login()
ftp.set_pasv(False)                       # RETR via PORT
with open("big.out", "wb") as fh:
    ftp.retrbinary("RETR big.bin", fh.write)
ftp.quit()
EOF
md5sum /tmp/ftp_root/big.bin big.out      # must match
rm -f big.out /tmp/ftp_root/big.bin
```

Expected: the transfer completes without crashes or aborts (`426`/`451`), and the `md5sum` of the source and destination are identical. Repeat the same test with `STOR` (upload, ~1 GB) and compare checksums.

#### 4.1.6 Multiple simultaneous clients (optional feature, +5)

With two terminals connected at the same time, run concurrent transfers:

```bash
# terminal A
python3 - <<'EOF'
from ftplib import FTP
ftp = FTP(); ftp.connect("127.0.0.1", 2121); ftp.login()
with open("dl_a.bin", "wb") as f:
    ftp.retrbinary("RETR big.bin", f.write)
ftp.quit()
EOF
# terminal B — at the same time
python3 - <<'EOF'
from ftplib import FTP
ftp = FTP(); ftp.connect("127.0.0.1", 2121); ftp.login()
with open("dl_b.bin", "wb") as f:
    ftp.retrbinary("RETR big.bin", f.write)
ftp.quit()
EOF
```

Expected: both transfers finish correctly and neither client starves. (Also counts toward the optional multi-client feature of §5.)

### 4.2 Client private tests (8 pts)

**Setup** — a standard FTP server acts as the remote host:

```bash
cd MyClient
mkdir -p /tmp/ftp_root
python std_server.py /tmp/ftp_root 10021 &     # standard pyftpdlib server
./client -ip 127.0.0.1 -port 10021             # interactive prompt
```

#### 4.2.1 `STOR` upload, content verified (2 pts)

Prepare a file, then at the client prompt issue `PASV` (or `PORT`) followed by `STOR`:

```bash
dd if=/dev/urandom of=up.bin bs=1k count=100
# client prompt:
#   USER anonymous
#   PASS anonymous
#   PASV
#   STOR up.bin
#   QUIT
md5sum up.bin /tmp/ftp_root/up.bin
```

Expected: the client prints the `150`/`125` mark then `226`; the two `md5sum`s match. Repeat once in PORT mode and once in PASV mode.

#### 4.2.2 `LIST` (1 pt)

```
LIST
```

Expected: the current directory listing (Linux `ls`-style, e.g. `-rw-r--r-- 1 ... up.bin`) is received through the data connection, with `150`/`125` then `226` from the client.

#### 4.2.3 `RMD` (1 pt)

```
MKD temp_dir
RMD temp_dir
```

Expected: `MKD` shows `257`, `RMD` shows `250`, and the directory is actually removed on the server (`ls /tmp/ftp_root`).

#### 4.2.4 Large file download & upload (~1 GB) (2 pts)

```bash
dd if=/dev/urandom of=/tmp/ftp_root/big.bin bs=1M count=1024
# client prompt:
#   PASV
#   RETR big.bin
#   PASV
#   STOR big.copy
#   QUIT
md5sum big.bin /tmp/ftp_root/big.bin /tmp/ftp_root/big.copy
```

Expected: transfers complete without hanging or aborting, with the correct `150`/`226` progression, and all `md5sum` values are identical.

#### 4.2.5 Command-line arguments (1 pt)

```bash
./client                                     # defaults to 127.0.0.1, port 21
./client -port 10021 -ip 127.0.0.1           # any order accepted
./client -ip 127.0.0.1 -port 10021
```

Expected: the client connects to the intended server in each case; without arguments it targets `127.0.0.1:21`.

#### 4.2.6 Standard / external server compatibility (1 pt)

`std_server.py` (a full `pyftpdlib` implementation) already exercises a non-custom server locally. For a real external host:

```bash
./client -ip ftp.example.com -port 21
USER anonymous
PASS you@example.com
LIST
RETR some-public-file
QUIT
```

Expected: login succeeds (`331` → `230`), replies are parsed correctly, and a downloaded file matches the server-side checksum.

---

## 5. Optional Features (+5 each, +10 max)

Per the project instructions, each completed optional feature should be demonstrated in a video included with the submission:

| Feature | Where | Quick check |
|---|---|---|
| Multiple simultaneous clients | server | see official project guidance |
| Resume after connection termination | client / server | interrupt a large `RETR`, reconnect, send `REST <offset>`, then `RETR` again; verify the remainder appends correctly |
| Non-blocking file transmission | server | start a large transfer and confirm other clients are still served immediately |
| User-friendly GUI | client | GUI still accepts command-line input as well |

---

## 6. Troubleshooting

| Symptom | Fix |
|---|---|
| `Permission denied` binding port 21 | run `./server` with `sudo` |
| `Address already in use` | `pkill -f './server'`, or use a different port with `-port` |
| `ModuleNotFoundError: pyftpdlib` | `pip install -r MyClient/requirements.txt` |
| `make` prints warnings | fix all warnings — warnings reduce credit |
| Transfers time out | use data ports in `20000–65535`; binary mode only (`TYPE I`) |
| Client connects but commands hang | confirm the server is healthy first (e.g. `nc -vz 127.0.0.1 <port>`) |
| Large-file transfer aborts | ensure transfers are loop-based and both sides stay in binary mode |

---

## 7. Score Summary

| # | Test | Points | Type |
|---|------|--------|------|
| S1 | Server build: `-Wall`, no warnings | 5 | Public |
| S2 | Server: `220`/`230`/SYST/TYPE | 8 | Public |
| S3 | Server: `RETR` (PORT) + `STOR` (PASV), content verified | 12 | Public |
| S4 | Server: `221` QUIT | 4 | Public |
| S5 | Server: `STOR` (PORT) / `RETR` (PASV) — both modes | 2 | Private |
| S6 | Server: `MKD`/`CWD`/`PWD`/`RMD` | 3 | Private |
| S7 | Server: path traversal / outside-root rejection | 2 | Private |
| S8 | Server: invalid input & error codes | 2 | Private |
| S9 | Server: ~1 GB large file | 2 | Private |
| C1 | Client: greeting / login / SYST / TYPE | 10 | Public |
| C2 | Client: `MKD`/`CWD`/`PWD` | 4 | Public |
| C3 | Client: PORT+RETR / PASV+RETR (content verified) | 8 | Public |
| C4 | Client: `STOR` upload verified | 2 | Private |
| C5 | Client: `LIST` | 1 | Private |
| C6 | Client: `RMD` | 1 | Private |
| C7 | Client: ~1 GB download / upload | 2 | Private |
| C8 | Client: CLI arguments & defaults | 1 | Private |
| C9 | Client: standard / external server login | 1 | Private |
| | **Server total** | **40** | |
| | **Client total** | **30** | |
| O1 | Optional features | +5 each (+10 max) | Optional |

> Sub-totals per the project instructions: server **29 public + 11 private = 40**; client **22 public + 8 private = 30**. The public rows approximate the coverage of the provided autograde scripts; the private rows are the local verification checklist for the remaining requirements.