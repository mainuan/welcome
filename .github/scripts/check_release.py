#!/usr/bin/env python3
"""Read-only release gates shared by local builds and GitHub Actions."""

import argparse
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile
import xml.etree.ElementTree as ET


EXPECTED_TESTS = {"tst_mainuan", "tst_appinstallation", "tst_layouts", "layouts_qml", "applications_qml", "smoke_startup"}
VERSION = r"[0-9]+\.[0-9]+(?:\.[0-9]+){0,2}"


def metadata(root, tag=""):
    cmake = (root / "CMakeLists.txt").read_text()
    match = re.search(r"project\(mainuan-welcome\s+VERSION\s+(" + VERSION + r")\s", cmake)
    if not match:
        raise ValueError("CMake must declare a numeric application version")
    version = match[1]
    first = (root / "debian/changelog").read_text().splitlines()[0]
    if not first.startswith(f"mainuan-welcome ({version}) "):
        raise ValueError("CMake and Debian changelog versions differ")
    appstream = ET.parse(root / "rootfs/usr/share/metainfo/org.mainuan.Welcome.metainfo.xml").getroot()
    release = appstream.find("releases/release")
    if release is None or release.get("version") != version:
        raise ValueError("AppStream must identify the same current release")
    if tag and (not re.fullmatch("v" + VERSION, tag) or tag != "v" + version):
        raise ValueError("Tag must exactly match the internal package version")
    # Historical documentation may mention reference captures; production inputs
    # must not require a developer's home or the external reference directory.
    inputs = [root / "CMakeLists.txt", *root.glob("src/**/*.cpp"), *root.glob("src/**/*.h"), *root.glob("qml/**/*.qml")]
    for path in inputs:
        if re.search(r"/home/[A-Za-z0-9_.-]+/|mainuan-layout-references", path.read_text()):
            raise ValueError(f"Personal build/runtime dependency in {path}")
    return version


def tests(report):
    cases = list(ET.parse(report).getroot().iter("testcase"))
    names = {case.get("name") for case in cases}
    if not EXPECTED_TESTS.issubset(names):
        raise ValueError(f"Required tests missing: {sorted(EXPECTED_TESTS - names)}")
    for case in cases:
        if case.find("skipped") is not None or case.find("failure") is not None or case.find("error") is not None:
            raise ValueError(f"Test failed or was skipped: {case.get('name')}")
        if case.get("status", "run") not in {"run", "passed"}:
            raise ValueError(f"Test did not run: {case.get('name')}")
    return len(cases)


def package(path, version):
    expected_name = f"mainuan-welcome_{version}_amd64.deb"
    if path.name != expected_name:
        raise ValueError(f"Expected package filename {expected_name}")
    def field(name):
        return subprocess.check_output(["dpkg-deb", "-f", str(path), name], text=True).strip()
    if [field(k) for k in ("Package", "Version", "Architecture")] != ["mainuan-welcome", version, "amd64"]:
        raise ValueError("Internal Debian name, version or architecture differs")
    if not field("Depends"):
        raise ValueError("Missing runtime dependencies")
    required = {
        "usr/bin/mainuan-welcome",
        "usr/libexec/mainuan-welcome/mainuan-welcome-helper",
        "usr/share/mainuan-welcome/freeoffice.profile",
        "usr/share/polkit-1/actions/org.mainuan.welcome.policy",
        "usr/share/applications/org.mainuan.Welcome.desktop",
        "usr/share/metainfo/org.mainuan.Welcome.metainfo.xml",
        "etc/xdg/autostart/org.mainuan.Welcome.desktop",
        "etc/mainuan/welcome.conf",
    }
    executables = {"usr/bin/mainuan-welcome", "usr/libexec/mainuan-welcome/mainuan-welcome-helper"}
    found = set()
    # Inspect the archive without extracting or executing anything in the package.
    with subprocess.Popen(["dpkg-deb", "--fsys-tarfile", str(path)], stdout=subprocess.PIPE) as process:
        with tarfile.open(fileobj=process.stdout, mode="r|*") as archive:
            for entry in archive:
                item = PurePosixPath(entry.name)
                if item.is_absolute() or ".." in item.parts:
                    raise ValueError("Unsafe package path")
                name = str(item)
                if name == ".":
                    continue
                if item.parts[0] not in {"usr", "etc"} or item.suffix in {".deb", ".ddeb", ".o", ".a", ".pyc"}:
                    raise ValueError(f"Unexpected development/personal file: {name}")
                if entry.uid != 0 or entry.gid != 0 or entry.mode & 0o022:
                    raise ValueError(f"Unsafe ownership/permissions: {name}")
                if name in required:
                    if not entry.isfile():
                        raise ValueError(f"Required file is not regular: {name}")
                    found.add(name)
                if name in executables and entry.mode & 0o111 != 0o111:
                    raise ValueError(f"Executable permission missing: {name}")
        if process.wait() != 0:
            raise ValueError("Cannot read package archive")
    if required - found:
        raise ValueError(f"Package files missing: {sorted(required - found)}")
    return expected_name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    info = sub.add_parser("metadata")
    info.add_argument("--root", type=Path, default=Path("."))
    info.add_argument("--tag", default="")
    info.add_argument("--github-output", type=Path)
    checks = sub.add_parser("tests")
    checks.add_argument("report", type=Path)
    deb = sub.add_parser("package")
    deb.add_argument("path", type=Path)
    deb.add_argument("--version", required=True)
    args = parser.parse_args()
    try:
        if args.command == "metadata":
            version = metadata(args.root, args.tag)
            if args.github_output:
                with args.github_output.open("a") as output:
                    output.write(f"version={version}\n")
            print(version)
        elif args.command == "tests":
            print(f"All {tests(args.report)} required tests ran successfully")
        else:
            if not re.fullmatch(VERSION, args.version):
                raise ValueError("Invalid expected package version")
            print(package(args.path, args.version))
    except (ValueError, OSError, ET.ParseError, tarfile.TarError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Release validation failed: {error}\n")


if __name__ == "__main__":
    main()
