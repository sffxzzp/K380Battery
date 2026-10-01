# Logitech K380 Battery Reader (C)

A small Windows command-line utility written in C that reads the battery status reported by the original Logitech K380 over Bluetooth HID++.

The keyboard reports its current discharge level and the next discharge level. The program displays those values as a range, such as `20%~50%`. This is a battery-level estimate reported by the keyboard, not a precise measurement or a remaining-time prediction.

## Supported device

- Logitech K380, Bluetooth HID (`VID_046D`, `PID_B342`)
- K380s is not currently supported.

The program opens the K380 HID++ long-report interface (usage page `0xFF00`, usage `0x0002`) and queries its Battery Status feature.

## Requirements

- Windows 10 or 11, 64-bit
- GCC
- CMake for Windows

## Build static HIDAPI 0.16.0

```
cmake -G "MinGW Makefiles" -DBUILD_SHARED_LIBS=false -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=Z:\build\x64 .
cmake --build . --target install
cmake -G "MinGW Makefiles" -DCMAKE_C_FLAGS="-m32" -DBUILD_SHARED_LIBS=false -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=Z:\build\x86 .
cmake --build . --target install
```

## Build the battery reader

From the repository root, in the same PowerShell window:

```
gcc main.c -o K380Battery-x64.exe -I include lib/x64/libhidapi.a -mconsole -s
gcc main.c -o K380Battery-x86.exe -I include lib/x86/libhidapi.a -mwindows -s -m32
```

This links the HIDAPI library statically and requests static linking for GCC runtime libraries. The executable does not need `hidapi.dll` or GCC runtime DLLs beside it. Windows system libraries, including the UCRT, remain normal Windows imports. `-mconsole` keeps the console visible for the output and keypress prompt.

## Go version

See the [Go implementation](../README.md).
