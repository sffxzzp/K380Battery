package main

import (
	"errors"
	"fmt"
	"os"
	"time"

	"github.com/sstallion/go-hid"
)

const (
	logitechVID uint16 = 0x046D
	k380PID     uint16 = 0xB342
	vendorPage  uint16 = 0xFF00
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "k380-battery:", err)
		os.Exit(1)
	}
}

func run() error {
	if err := hid.Init(); err != nil {
		return fmt.Errorf("initialize HIDAPI: %w", err)
	}
	defer hid.Exit()

	var selected *hid.DeviceInfo
	var found []hid.DeviceInfo
	err := hid.Enumerate(logitechVID, k380PID, func(info *hid.DeviceInfo) error {
		found = append(found, *info)
		// Logitech exposes the HID++ 2.0 long-report endpoint as usage 0x0002.
		// Usage 0x0001 is the short-report endpoint used for Fn-lock commands.
		if selected == nil && info.UsagePage == vendorPage && info.Usage == 0x0002 {
			copy := *info
			selected = &copy
		}
		return nil
	})
	if err != nil {
		return fmt.Errorf("enumerate K380 HID interfaces: %w", err)
	}
	if selected == nil {
		if len(found) == 0 {
			return errors.New("K380 HID interface not found; make sure the keyboard is paired and connected")
		}
		for _, info := range found {
			fmt.Fprintf(os.Stderr, "found K380 HID interface: usagePage=0x%04X usage=0x%04X path=%s\n", info.UsagePage, info.Usage, info.Path)
		}
		return errors.New("K380 HID++ long-report interface (usage page 0xFF00, usage 0x0002) was not enumerated")
	}

	device, err := hid.OpenPath(selected.Path)
	if err != nil {
		return fmt.Errorf("open K380 HID interface: %w", err)
	}
	defer device.Close()
	fmt.Fprintf(os.Stderr, "using K380 HID interface: usagePage=0x%04X usage=0x%04X interface=%d\n", selected.UsagePage, selected.Usage, selected.InterfaceNbr)

	// Bluetooth HID++ 2.0 calls use the 20-byte long report (0x11).
	// Feature 0x06 is Battery Status; function 0x00 uses software ID 0x0A.
	request := make([]byte, 20)
	copy(request, []byte{0x11, 0xFF, 0x06, 0x0A})
	if n, err := device.Write(request); err != nil {
		return fmt.Errorf("send long battery query: %w", err)
	} else if n != len(request) {
		return fmt.Errorf("short write sending long battery query: wrote %d of %d bytes", n, len(request))
	}

	deadline := time.Now().Add(3 * time.Second)
	received := 0
	for time.Now().Before(deadline) {
		response := make([]byte, 64)
		n, err := device.ReadWithTimeout(response, 400*time.Millisecond)
		if errors.Is(err, hid.ErrTimeout) {
			continue
		}
		if err != nil {
			return fmt.Errorf("read battery response: %w", err)
		}
		received++
		fmt.Fprintf(os.Stderr, "HID report (%d bytes): % X\n", n, response[:n])

		// K380's short response normally carries the command/feature byte at
		// [2] and percentage at [4]. Some HID backends include a leading
		// zero report-ID byte, shifting those fields by one.
		level, ok := batteryLevel(response[:n])
		if ok {
			if level > 100 {
				return fmt.Errorf("K380 returned an invalid battery level: %d", level)
			}
			fmt.Printf("K380 battery: %d%%\n", level)
			return nil
		}
	}

	return fmt.Errorf("timed out waiting for a K380 battery response (%d HID report(s) received)", received)
}

func batteryLevel(report []byte) (int, bool) {
	if len(report) >= 5 && report[2] == 0x06 {
		return int(report[4]), true
	}
	if len(report) >= 6 && report[0] == 0x00 && report[3] == 0x06 {
		return int(report[5]), true
	}
	return 0, false
}
