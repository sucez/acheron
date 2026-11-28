"""Build and independently verify the assets attached to a Windows release."""
import argparse
import hashlib
import json
import re
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

REPOSITORY = "https://github.com/sucez/acheron"
NUMBER = r"(?:0|[1-9][0-9]*)"


def identity(version, channel, commit):
    suffix = {"stable": "", "preview": rf"-preview\.{NUMBER}"}.get(channel)
    if suffix is None or not re.fullmatch(rf"{NUMBER}\.{NUMBER}\.{NUMBER}{suffix}", version):
        raise ValueError("Invalid release version/channel")
    if not re.fullmatch("[a-f0-9]{40}", commit):
        raise ValueError("Expected a full Git commit")
    return dict(version=version, channel=channel, commit=commit, architecture="x64")


def encode(value):
    return json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + "\n"


def asset_name(info):
    return f"Acheron-Custom-{info['version']}-windows-x64.zip"


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def manifest(path, info):
    if path.name != asset_name(info):
        raise ValueError("Incorrect asset name")
    with ZipFile(path) as archive:
        if archive.testzip() or json.loads(archive.read("build-info.json")) != info:
            raise ValueError("Invalid ZIP or mismatched build identity")
    tag = "v" + info["version"]
    package = dict(os="windows", architecture="x64", filename=path.name,
                   size=path.stat().st_size, sha256=digest(path),
                   url=f"{REPOSITORY}/releases/download/{tag}/{path.name}")
    return dict(schema_version=1, version=info["version"], channel=info["channel"], commit=info["commit"],
                release_notes=f"{REPOSITORY}/releases/tag/{tag}", packages=[package])


def create(stage, output, info):
    if json.loads((stage / "build-info.json").read_text(encoding="utf-8")) != info:
        raise ValueError("Executable identity differs from release request")
    output.mkdir(parents=True, exist_ok=True)
    path = output / asset_name(info)
    with ZipFile(path, "x", compression=ZIP_DEFLATED) as archive:
        for file in sorted(stage.rglob("*")):
            if file.is_file():
                archive.write(file, file.relative_to(stage))
    data = manifest(path, info)
    (output / "manifest.json").write_bytes(encode(data).encode())
    (output / "SHA256SUMS").write_bytes(checksum_line(data))


def checksum_line(data):
    package, = data["packages"]
    return f"{package['sha256']}  {package['filename']}\n".encode()


def verify(output, info):
    data = manifest(output / asset_name(info), info)
    if (output / "manifest.json").read_bytes() != encode(data).encode():
        raise ValueError("Manifest differs from canonical artifact metadata")
    if (output / "SHA256SUMS").read_bytes() != checksum_line(data):
        raise ValueError("Invalid checksum file")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("operation", choices=["check", "create", "verify"])
    parser.add_argument("version")
    parser.add_argument("channel")
    parser.add_argument("commit")
    parser.add_argument("--stage", type=Path, default=Path("stage"))
    parser.add_argument("--output", type=Path, default=Path("dist"))
    args = parser.parse_args()
    info = identity(args.version, args.channel, args.commit)
    if args.version != Path("VERSION").read_text().strip():
        parser.error("Requested version differs from VERSION")
    if args.operation == "create":
        create(args.stage, args.output, info)
    if args.operation != "check":
        verify(args.output, info)
