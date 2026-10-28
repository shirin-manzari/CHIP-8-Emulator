# CHIP-8 Emulator

Build, test, and run in **MSYS2 UCRT64**:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL3
make
make test
./chip8.exe "your-game.ch8"
```

Supply your own ROM and replace `your-game.ch8` with its path.
Controls: `1234`, `QWER`, `ASDF`, `ZXCV` (varies by game). **Escape** exits.
