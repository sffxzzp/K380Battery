#define _WIN32_WINNT 0x0600

#include <hidapi.h>
#include <windows.h>
#include <conio.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

enum {
    LOGITECH_VID = 0x046D,
    K380_PID = 0xB342,
    VENDOR_USAGE_PAGE = 0xFF00,
    HIDPP_LONG_USAGE = 0x0002,
    BATTERY_FEATURE_INDEX = 0x06,
    SOFTWARE_ID = 0x0A,
    LONG_REPORT_ID = 0x11,
    LONG_REPORT_SIZE = 20,
    MAX_REPORT_SIZE = 64,
    QUERY_TIMEOUT_MS = 3000,
    READ_TIMEOUT_MS = 400
};

static void print_hid_error(const char *operation, hid_device *device)
{
    const wchar_t *wide_message = hid_error(device);
    char *utf8_message;
    int utf8_size;

    if (wide_message == NULL || wide_message[0] == L'\0') {
        fprintf(stderr, "%s failed\n", operation);
        return;
    }

    utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide_message, -1, NULL, 0, NULL, NULL);
    if (utf8_size <= 0) {
        fprintf(stderr, "%s failed\n", operation);
        return;
    }

    utf8_message = (char *)malloc((size_t)utf8_size);
    if (utf8_message == NULL) {
        fprintf(stderr, "%s failed\n", operation);
        return;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wide_message, -1, utf8_message, utf8_size, NULL, NULL) > 0) {
        fprintf(stderr, "%s: %s\n", operation, utf8_message);
    } else {
        fprintf(stderr, "%s failed\n", operation);
    }

    free(utf8_message);
}

static int decode_battery_levels(const unsigned char *report, size_t length,
                                 int *current_level, int *next_level)
{
    if (length >= 6 && report[2] == BATTERY_FEATURE_INDEX) {
        *current_level = report[4];
        *next_level = report[5];
        return 1;
    }

    /* Some HID backends include a leading zero report-ID byte. */
    if (length >= 7 && report[0] == 0x00 && report[3] == BATTERY_FEATURE_INDEX) {
        *current_level = report[5];
        *next_level = report[6];
        return 1;
    }

    return 0;
}

int main(void)
{
    struct hid_device_info *devices = NULL;
    struct hid_device_info *selected = NULL;
    struct hid_device_info *info;
    hid_device *device = NULL;
    unsigned char request[LONG_REPORT_SIZE] = {0};
    int exit_code = 1;
    int hid_initialized = 0;
    int received = 0;

    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "Starting...\n");
    fprintf(stderr, "Initializing HIDAPI...\n");
    if (hid_init() != 0) {
        fprintf(stderr, "k380-battery: initialize HIDAPI failed\n");
        goto cleanup;
    }
    hid_initialized = 1;

    fprintf(stderr, "Searching for K380 HID interface...\n");
    devices = hid_enumerate(LOGITECH_VID, K380_PID);
    for (info = devices; info != NULL; info = info->next) {
        if (selected == NULL && info->usage_page == VENDOR_USAGE_PAGE &&
            info->usage == HIDPP_LONG_USAGE) {
            selected = info;
        }
    }

    if (selected == NULL) {
        if (devices == NULL) {
            fprintf(stderr,
                    "k380-battery: K380 HID interface not found; make sure the keyboard is paired and connected\n");
        } else {
            for (info = devices; info != NULL; info = info->next) {
                fprintf(stderr,
                        "found K380 HID interface: usagePage=0x%04X usage=0x%04X path=%s\n",
                        info->usage_page, info->usage,
                        info->path != NULL ? info->path : "(unknown)");
            }
            fprintf(stderr,
                    "k380-battery: K380 HID++ long-report interface (usage page 0xFF00, usage 0x0002) was not enumerated\n");
        }
        goto cleanup;
    }

    device = hid_open_path(selected->path);
    if (device == NULL) {
        print_hid_error("k380-battery: open K380 HID interface", NULL);
        goto cleanup;
    }

    fprintf(stderr,
            "using K380 HID interface: usagePage=0x%04X usage=0x%04X interface=%d\n",
            selected->usage_page, selected->usage, selected->interface_number);

    /* HID++ 2.0 Battery Status: long report, feature index 0x06, function 0. */
    request[0] = LONG_REPORT_ID;
    request[1] = 0xFF;
    request[2] = BATTERY_FEATURE_INDEX;
    request[3] = SOFTWARE_ID;

    {
        int written = hid_write(device, request, sizeof(request));
        if (written < 0) {
            print_hid_error("k380-battery: send battery query", device);
            goto cleanup;
        }
        if (written != (int)sizeof(request)) {
            fprintf(stderr,
                    "k380-battery: short write sending battery query: wrote %d of %zu bytes\n",
                    written, sizeof(request));
            goto cleanup;
        }
    }

    {
        ULONGLONG deadline = GetTickCount64() + QUERY_TIMEOUT_MS;
        while (GetTickCount64() < deadline) {
            unsigned char response[MAX_REPORT_SIZE] = {0};
            ULONGLONG remaining = deadline - GetTickCount64();
            int timeout = remaining < READ_TIMEOUT_MS ? (int)remaining : READ_TIMEOUT_MS;
            int length = hid_read_timeout(device, response, sizeof(response), timeout);

            if (length < 0) {
                print_hid_error("k380-battery: read battery response", device);
                goto cleanup;
            }
            if (length == 0) {
                continue;
            }

            ++received;
            fprintf(stderr, "HID report (%d bytes):", length);
            for (int i = 0; i < length; ++i) {
                fprintf(stderr, " %02X", response[i]);
            }
            fputc('\n', stderr);

            {
                int current_level;
                int next_level;
                if (decode_battery_levels(response, (size_t)length,
                                          &current_level, &next_level)) {
                    int low;
                    int high;
                    if (current_level > 100 || next_level > 100) {
                        fprintf(stderr,
                                "k380-battery: invalid battery levels: current=%d next=%d\n",
                                current_level, next_level);
                        goto cleanup;
                    }

                    low = current_level < next_level ? current_level : next_level;
                    high = current_level > next_level ? current_level : next_level;
                    printf("K380 battery: %d%%~%d%%\n", low, high);
                    exit_code = 0;
                    goto cleanup;
                }
            }
        }
    }

    fprintf(stderr,
            "k380-battery: timed out waiting for a K380 battery response (%d HID report(s) received)\n",
            received);

cleanup:
    if (device != NULL) {
        hid_close(device);
    }
    if (devices != NULL) {
        hid_free_enumeration(devices);
    }
    if (hid_initialized) {
        hid_exit();
    }
    fflush(stdout);
    fprintf(stderr, "Press any key to exit...\n");
    (void)_getch();
    return exit_code;
}
