# Contributing to reTerminal Sticky Bunny

Thank you for helping Sticky Bunny grow. Contributions are welcome across firmware behavior, ePaper UI, pixel assets, documentation and hardware validation.

## Before you start

1. Open an issue describing the user-facing outcome.
2. For hardware behavior, identify the matching implementation in `Sticky_dashboard_demo` or the relevant component datasheet.
3. Keep each pull request focused on one feature or repair.

## Development setup

```bash
git clone https://github.com/limengdu/reTerminal_Sticky_Bunny.git
cd reTerminal_Sticky_Bunny
pio run -e sticky-debug
```

The release configuration is the repository default:

```bash
pio run -e sticky-release
```

## Code style

- Follow the existing module boundaries and naming style.
- Keep identifiers, UI strings and logs in English.
- Document public interfaces and complex logic with concise English and Chinese comments.
- Store hardware-independent rules in pure C++ modules so they can run on a computer.
- Reuse the shared `Canvas`, one-bit asset pipeline and app lifecycle interfaces.
- Add a configurable log switch for diagnostics that would be noisy in release firmware.

## Tests

Run every host regression test:

```bash
./tools/run_host_tests.sh
```

Then build all supported profiles:

```bash
pio run -e sticky-debug
pio run -e sticky-power-test
pio run -e sticky-release
```

Hardware-dependent changes should include the device, firmware profile, exact interaction steps, expected screen or serial result, and whether the device was powered from USB or battery.

## Assets

Editable source artwork belongs in `assets/`. Generated one-bit C++ assets belong in `src/ui/assets/`. Run the matching `tools/generate_*.py` script and commit both the source and generated result when a visual asset changes.

Install the deterministic image-conversion dependency with:

```bash
python3 -m pip install -r requirements-dev.txt
```

## Documentation

Update `README.md` and `README_CN.md` whenever a user-visible workflow, build command, configuration, screenshot, or architecture boundary changes. Use current code-rendered images for feature documentation and keep concept art in the README design-history gallery.

## Pull requests

A ready pull request contains:

- a concise explanation of the behavior;
- linked issue or motivation;
- host-test and PlatformIO build results;
- device validation steps for hardware-dependent work;
- before-and-after images for visible ePaper changes;
- documentation updates when the public behavior changes.
