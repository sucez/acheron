"""Stage the full MinSizeRel build in a fresh directory on the Windows runner."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from distribution import encode, identity


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def notices(source, destination):
    count = 0
    for file in source.rglob("*"):
        if file.is_file() and file.name.lower().startswith(("license", "licence", "copying", "copyright", "notice")):
            copy(file, destination / file.relative_to(source))
            count += 1
    return count


def stage(info):
    binary = Path("build/MinSizeRel")
    target = Path("stage").resolve()
    target.mkdir()  # Never reuse a directory containing previous outputs.
    exe = target / "acheron.exe"
    copy(binary / exe.name, exe)
    for file in [*binary.glob("*.dll"), *Path("curl-impersonate/lib").glob("*.dll"),
                 *Path(os.environ["VCToolsRedistDir"]).glob("x64/Microsoft.VC*.CRT/*.dll")]:
        copy(file, target / file.name)
    subprocess.run(["windeployqt", "--release", "--no-translations", "--no-opengl-sw",
                    "--no-system-d3d-compiler", "--no-compiler-runtime", str(exe)], check=True)
    for item in ["fonts/TwemojiCOLRv0.ttf", "certs/cacert.pem"]:
        copy(binary / item, target / item)
    for pattern in ["Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll", "Qt6Svg.dll",
                    "platforms/qwindows.dll", "sqldrivers/qsqlite.dll", "tls/qschannelbackend.dll",
                    "*curl*impersonate*.dll", "avcodec*.dll", "avformat*.dll", "avutil*.dll",
                    "swresample*.dll", "swscale*.dll", "msvcp140.dll", "vcruntime140.dll"]:
        if not any(target.glob(pattern)):
            raise ValueError(f"Missing runtime component: {pattern}")
    for plugin in (target / "sqldrivers").glob("*.dll"):
        if plugin.name.lower() != "qsqlite.dll":
            plugin.unlink()
    licenses = target / "licenses"
    shutil.copytree("packaging/licenses", licenses)
    copy(Path("LICENSE.txt"), target / "LICENSE.txt")
    copy(Path("packaging/THIRD-PARTY.md"), target / "THIRD-PARTY.md")
    share = Path("build/vcpkg_installed/x64-windows-static-md/share")
    if not notices(share, licenses / "vcpkg"):
        raise ValueError("Missing vcpkg notices")
    for name in ["libdave", "mlspp", "rnnoise", "emoji-segmenter", "qtkeychain", "miniaudio"]:
        if not notices(Path("vendor") / name, licenses / name):
            raise ValueError(f"Missing notices for {name}")
    # Nayuki embeds the MIT grant directly in its source header.
    copy(Path("vendor/qrcodegen/cpp/qrcodegen.hpp"), licenses / "qrcodegen.txt")
    notices(Path("ffmpeg"), licenses / "ffmpeg")
    notices(Path("curl-impersonate"), licenses / "curl-impersonate")
    for root in [Path(os.environ["QT_ROOT_DIR"]) / "licenses", Path(os.environ["QT_ROOT_DIR"]).parent.parent / "Licenses"]:
        if root.is_dir():
            shutil.copytree(root, licenses / "Qt", dirs_exist_ok=True)
    # Isolate the probe from the installed Qt kit and compiler runtime on PATH.
    env = {k: os.environ[k] for k in ("SYSTEMROOT", "TEMP", "TMP")}
    env["PATH"] = str(Path(env["SYSTEMROOT"]) / "System32")
    output = subprocess.check_output([str(exe), "--build-info"], cwd=target, env=env, timeout=30)
    if json.loads(output) != info:
        raise ValueError("Staged executable does not match requested release")
    (target / "build-info.json").write_bytes(encode(info).encode())


if __name__ == "__main__":
    stage(identity(*sys.argv[1:]))
