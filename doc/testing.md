# Testing & Auto-grading

The repository includes two helper test scripts that mirror the public test cases.

## Server test (`MyServer/autograde_server.py`)

- `make` must succeed with no warnings (`-Wall` checked) or credit is reduced.
- Starts `./server` (with optional `-port`/`-root`), then uses Python's standard `ftplib` to verify:
  - `220` greeting, `230` login, `215 UNIX Type: L8` (SYST), `200 Type set to I.` (TYPE I)
  - **RETR (PORT mode)** — file content must match exactly (`filecmp`)
  - **STOR (PASV mode)** — uploaded file content must match
  - `221` (QUIT)
- Runs twice: once with default args (port 21 via `sudo`, root `/tmp`) and once with a random port and a random temporary root directory.

## Client test (`MyClient/autograde_client.py`)

- Runs `std_server.py` (a standard `pyftpdlib` FTP server — `pip install -r requirements.txt` first) on port `10021`.
- Starts `./client -ip 127.0.0.1 -port 10021` and feeds scripted commands via stdin, then checks the client's printed responses and the server's actions:
  - `220` greeting
  - `USER anonymous`/`PASS` login (`331` then `230`)
  - `SYST` → `215`, `TYPE I` → `200`
  - `MKD` → `257`, `CWD` → `250`, `PWD` → `257`
  - `PORT` → `200` then `RETR` (download, content verified with `filecmp`)
  - `PASV` → `227` then `RETR` (download, content verified)

These scripts are local conveniences to check public-test compatibility before submission.