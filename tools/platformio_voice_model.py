"""PlatformIO upload integration and safety checks for the ESP-SR model.

ESP-SR's CMake target generates the model image. This post script keeps the
special upload behavior isolated from all production environments.
"""

import csv
from pathlib import Path
import subprocess

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


FLASH_SIZE = 32 * 1024 * 1024


def parse_size(value: str) -> int:
    value = value.strip()
    if value.lower().startswith("0x"):
        return int(value, 16)
    if value[-1:].upper() == "K":
        return int(value[:-1]) * 1024
    if value[-1:].upper() == "M":
        return int(value[:-1]) * 1024 * 1024
    return int(value)


def partition(project_dir: Path, name: str) -> tuple[int, int]:
    with (project_dir / "partitions.voice.csv").open(
        encoding="utf-8", newline=""
    ) as partition_file:
        for row in csv.reader(partition_file):
            if not row or row[0].strip().startswith("#"):
                continue
            if row[0].strip() == name:
                return parse_size(row[3]), parse_size(row[4])
    raise RuntimeError(f"partition not found: {name}")


def generate_voice_model(target, source, env):
    del source
    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_dir = Path(env.subst("$BUILD_DIR"))
    component_dir = project_dir / "managed_components" / "espressif__esp-sr"
    generator = component_dir / "model" / "movemodel.py"
    sdkconfig = project_dir / f"sdkconfig.{env.subst('$PIOENV')}"
    subprocess.run(
        [
            env.subst("$PYTHONEXE"),
            str(generator),
            "-d1",
            str(sdkconfig),
            "-d2",
            str(component_dir),
            "-d3",
            str(build_dir),
        ],
        check=True,
    )
    expected = Path(str(target[0]))
    if not expected.is_file():
        raise RuntimeError("ESP-SR generator completed without srmodels.bin")


def validate_voice_model(source, target, env):
    del source, target
    project_dir = Path(env.subst("$PROJECT_DIR"))
    model_offset, model_partition_size = partition(project_dir, "model")
    app_offset, app_partition_size = partition(project_dir, "factory")
    model_path = Path(
        env.subst("$BUILD_DIR/srmodels/srmodels.bin")
    )
    if not model_path.is_file():
        raise RuntimeError(
            "ESP-SR model image is missing; refusing to create an uploadable "
            "voice-classifier build"
        )
    model_size = model_path.stat().st_size
    if model_size <= 0 or model_size > model_partition_size:
        raise RuntimeError(
            f"ESP-SR model size {model_size} exceeds partition "
            f"{model_partition_size}"
        )
    if model_offset < app_offset + app_partition_size:
        raise RuntimeError("ESP-SR model partition overlaps application")
    if model_offset + model_partition_size > FLASH_SIZE:
        raise RuntimeError("ESP-SR model partition exceeds 32 MiB flash")
    if int(env.subst("$ESP32_APP_OFFSET"), 0) != app_offset:
        raise RuntimeError("PlatformIO application offset disagrees with CSV")
    print(
        "Voice model verified: "
        f"path={model_path} bytes={model_size} "
        f"offset=0x{model_offset:X} partition={model_partition_size}"
    )


program_image = env.subst("$BUILD_DIR/${PROGNAME}.bin")
model_image = env.subst("$BUILD_DIR/srmodels/srmodels.bin")
project_dir = Path(env.subst("$PROJECT_DIR"))
model_offset, _ = partition(project_dir, "model")
component_manifest = env.subst("$PROJECT_DIR/src/idf_component.yml")
model_generator = env.subst(
    "$PROJECT_DIR/managed_components/espressif__esp-sr/model/movemodel.py"
)
model_node = env.Command(
    model_image,
    [
        component_manifest,
        model_generator,
        env.subst("$PROJECT_DIR/sdkconfig.voice.defaults"),
        env.subst("$PROJECT_DIR/partitions.voice.csv"),
        env.subst("$PROJECT_DIR/sdkconfig.${PIOENV}"),
    ],
    generate_voice_model,
)
env.Depends(program_image, model_node)
env.AddPostAction(program_image, validate_voice_model)

# The espressif32 builder has already assembled UPLOADERFLAGS before post
# scripts run. Extend that final command directly; do not modify board-global
# FLASH_EXTRA_IMAGES or production profiles.
env.Append(UPLOADERFLAGS=[f"0x{model_offset:X}", model_image])
