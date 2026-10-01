# Logitech K380 Battery Reader (Go)

A small Windows command-line utility that reads the battery status reported by the original Logitech K380 over Bluetooth HID++.

The keyboard reports its current discharge level and the next discharge level. The program displays those values as a range, such as `20%~50%`. This is a battery-level estimate reported by the keyboard, not a precise measurement or a remaining-time prediction.

## Supported device

- Logitech K380, Bluetooth HID (`VID_046D`, `PID_B342`)
- K380s is not currently supported.

The program opens the K380 HID++ long-report interface (usage page `0xFF00`, usage `0x0002`) and queries its Battery Status feature.

## Requirements

- Windows 10 or 11, 64-bit
- Go 1.20 or later for Windows
- GCC (needed by the Go HID library's cgo code)

## Build on Windows

```powershell
$env:CGO_ENABLED = '1'

gcc --version
go env GOOS GOARCH
go build -o .\k380-battery.exe .
```

Use 64-bit Go with the x86_64 GCC toolchain. `go build` downloads the Go module dependencies as needed.

## Run

Pair and connect the K380 over Bluetooth, then run:

```powershell
.\k380-battery.exe
```

Example output:

```text
K380 battery: 20%~50%
```

## C version

The repository also includes a C implementation. See the [C version build instructions](c/README.md).
