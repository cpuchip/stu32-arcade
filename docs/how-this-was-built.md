# How this was built

The code, models, benches and documents here were written by Claude, working as several Claude Code sessions that
each had one job. The sessions are called seats, and each seat has a name. Michael Stufflebeam owns the project,
decides what gets built and how it should play, and makes every call on names, licences and what is published. Every
commit is under his account, and a commit whose content Claude wrote ends with a `Co-Authored-By: Claude` line.

The history and the documents keep the seats' names, so a reader can see how the work was divided. The seats that
appear:

- **arcade** wrote the games, their Python models, the host runner, the WebAssembly bridge and the device benches.
- **abacus** wrote the firmware work units, reviewed each game's design before its code and each unit before it was
  accepted, and carried Michael's rulings to the other seats.
- **soroban** maintains the calculator's firmware: it defined the app interface (`apps.h`), applied each game to the
  firmware, and ran the benches on its ESP32-S3 board.
- **casim** works on the calculator's computer algebra; its XIAO ESP32-S3 board ran the first WebAssembly
  measurements in `docs/wasm-probe.md`.
- **basecamp** coordinates the seats, and relayed some of Michael's answers early on.
- **primer** and **tally** (a web page and an Android app for the same calculator) appear only where their builds
  constrained a choice here.

`lepton` and `fermion` are the names of the two machines the seats ran on.

Some references in the documents and commit messages point at records that are not public:

- `#` and a four-digit number (`#4499`) is a message in the seats' shared chat;
- "decision" and a number is one of Michael's numbered rulings;
- "unit" and a three-digit number (`unit 065`) is a firmware work unit, written up in the firmware repository's
  `work/` directory.

How each game was checked is in the README; each game's design document says what was measured and on which board.
