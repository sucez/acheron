"""Sign or verify the exact manifest bytes using OpenSSL 3 and an explicit PEM key."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile


def run(*args):
    return subprocess.check_output(["openssl", *map(str, args)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=["sign", "verify"])
    parser.add_argument("manifest", type=Path)
    parser.add_argument("key", type=Path)
    parser.add_argument("signature", type=Path)
    args = parser.parse_args()
    sign = args.operation == "sign"
    der = run("pkey", *([] if sign else ["-pubin"]), "-in", args.key, "-pubout", "-outform", "DER")
    if len(der) != 44 or der[:12] != bytes.fromhex("302a300506032b6570032100"):
        parser.error("Expected an Ed25519 key")
    prefix = hashlib.sha256(der[12:]).hexdigest().encode() + b"\n"
    if sign:
        signature = run("pkeyutl", "-sign", "-rawin", "-inkey", args.key, "-in", args.manifest)
        if len(signature) != 64:
            parser.error("Invalid Ed25519 signature length")
        with args.signature.open("xb") as file:
            file.write(prefix + signature)
    else:
        data = args.signature.read_bytes()
        if len(data) != 129 or not data.startswith(prefix):
            parser.error("Invalid signature envelope or key fingerprint")
        with tempfile.TemporaryDirectory() as tmp:
            sig = Path(tmp) / "sig"
            sig.write_bytes(data[65:])
            run("pkeyutl", "-verify", "-rawin", "-pubin", "-inkey", args.key, "-in", args.manifest, "-sigfile", sig)


if __name__ == "__main__":
    main()
