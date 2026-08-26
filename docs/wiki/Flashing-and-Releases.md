# Flashing and Releases

Version `0.1.0` produces an application image and a complete fresh-install image.

## PlatformIO upload

For an existing development checkout:

```bash
pio run -e sticky-release -t upload
```

This uses the board definition and partition table in the repository.

## Release images

| File | Address | User data |
| --- | ---: | --- |
| `sticky-0.1.0-app.bin` | `0x10000` | Retains the existing NVS partition |
| `sticky-0.1.0-full.bin` | `0x0` | Initializes bootloader, partitions and user data |

Fresh installation:

```bash
python -m esptool --chip esp32s3 --port <PORT> --baud 921600 \
  write_flash 0x0 sticky-0.1.0-full.bin
```

Application upgrade:

```bash
python -m esptool --chip esp32s3 --port <PORT> --baud 921600 \
  write_flash 0x10000 sticky-0.1.0-app.bin
```

Replace `<PORT>` with the serial device reported by the operating system.

## Verify downloaded files

Release packages include `SHA256SUMS.txt`:

```bash
shasum -a 256 -c SHA256SUMS.txt
```

Every line should report `OK`.

## Release gate

A release is ready when:

1. Asset generators create no unexplained diff.
2. Native tests pass.
3. Release, debug and power-test environments build.
4. Firmware identity contains the release version without a development suffix.
5. Application and complete images pass esptool validation and SHA-256 checks.
6. A physical device completes cold boot, tutorial touch, launcher input, all four apps, sleep/wake and battery-only operation.
7. README, Wiki, changelog and third-party notices match the shipped behavior.
