#!/usr/bin/env python3
"""Fail closed on non-baseline amd64 build inputs and distributed ELF notes.

ELF notes describe declared requirements, not every instruction or dispatched
implementation. Pair this audit with controlled flags and the QEMU startup gate.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile

ISA_NEEDED = 0xC0008002


def elf_requirement(path):
    data = Path(path).read_bytes()
    if data[:4] != b'\x7fELF':
        return None
    if len(data) < 64 or data[4:6] != b'\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 62:
        raise ValueError(f'{path}: expected little-endian ELF64 x86-64')
    regions = set()
    phoff, shoff = struct.unpack_from('<QQ', data, 32)
    phsize, phnum, shsize, shnum = struct.unpack_from('<HHHH', data, 54)
    for table, size, count, note_type, section in ((phoff, phsize, phnum, 4, False), (shoff, shsize, shnum, 7, True)):
        if count and (size < (64 if section else 56) or table + size * count > len(data)):
            raise ValueError(f'{path}: malformed ELF table')
        for index in range(count):
            pos = table + size * index
            kind = struct.unpack_from('<I', data, pos + (4 if section else 0))[0]
            if kind == note_type:
                offset = struct.unpack_from('<Q', data, pos + (24 if section else 8))[0]
                length = struct.unpack_from('<Q', data, pos + 32)[0]
                if offset + length > len(data):
                    raise ValueError(f'{path}: truncated ELF note')
                regions.add((offset, length))
    needed = 0
    declared = False
    for start, length in regions:
        end, pos = start + length, start
        while pos + 12 <= end:
            namesz, descsz, kind = struct.unpack_from('<III', data, pos)
            name_start = pos + 12
            desc_start = name_start + ((namesz + 3) & ~3)
            next_pos = desc_start + ((descsz + 3) & ~3)
            if next_pos > end:
                raise ValueError(f'{path}: malformed note payload')
            if data[name_start:name_start + namesz] == b'GNU\0' and kind == 5:
                prop = desc_start
                while prop + 8 <= desc_start + descsz:
                    prop_type, prop_size = struct.unpack_from('<II', data, prop)
                    payload = prop + 8
                    if payload + prop_size > desc_start + descsz:
                        raise ValueError(f'{path}: malformed GNU property')
                    if prop_type == ISA_NEEDED:
                        if prop_size != 4:
                            raise ValueError(f'{path}: invalid ISA property size')
                        needed |= struct.unpack_from('<I', data, payload)[0]
                        declared = True
                    prop = payload + ((prop_size + 7) & ~7)
            pos = next_pos
    if needed & ~1:
        raise ValueError(f'{path}: GNU_PROPERTY_X86_ISA_1_NEEDED={needed:#x} exceeds x86-64-baseline')
    return {'path': str(path), 'needed_mask': needed, 'declared': declared}


def installed_variants(text):
    return [line for line in text.splitlines() if len(line.split('\t')) != 4
            or line.split('\t')[2] not in ('', 'amd64')]


def environment():
    arch = subprocess.check_output(['dpkg', '--print-architecture'], text=True).strip()
    if arch != 'amd64':
        raise ValueError(f'Expected amd64 build environment, got {arch}')
    config = subprocess.check_output(['apt-config', 'dump'], text=True)
    for line in config.splitlines():
        if line.lower().startswith('apt::architecture-variants') and re.search(r'"[^"]*[^"\s][^"]*"', line):
            raise ValueError(f'APT CPU variants enabled: {line}')
    packages = subprocess.check_output(['dpkg-query', '-W', '-f=${binary:Package}\t${Architecture}\t${Architecture-Variant}\t${Version}\n'], text=True)
    bad = installed_variants(packages)
    if bad:
        raise ValueError('Installed architecture variants: ' + '; '.join(bad))
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    defaults = subprocess.check_output(compiler + ['-Q', '--help=target'], text=True)
    if not re.search(r'-march=\s+x86-64\s', defaults):
        raise ValueError('Compiler default target is not x86-64 baseline')
    startup = []
    for name in ('Scrt1.o', 'crt1.o', 'crti.o', 'crtn.o', 'crtbeginS.o', 'crtendS.o'):
        path = subprocess.check_output(compiler + ['-print-file-name=' + name], text=True).strip()
        if not Path(path).is_file():
            raise ValueError(f'Missing compiler startup input: {name}')
        startup.append(elf_requirement(path))
    return {'architecture': arch, 'apt_config': config, 'packages': packages,
            'compiler': subprocess.check_output(compiler + ['--version'], text=True),
            'compiler_configuration': subprocess.check_output(compiler + ['-v'], stderr=subprocess.STDOUT, text=True),
            'repository_policy': subprocess.check_output(['apt-cache', 'policy', 'gcc', 'g++', 'libc6-dev', 'libqt6core6t64', 'libkf6configcore6'], text=True),
            'compiler_defaults': defaults, 'startup_objects': startup,
            'dpkg_buildflags': subprocess.check_output(['dpkg-buildflags'], text=True)}


def audit_tree(root):
    result = []
    for path in sorted(Path(root).rglob('*')):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open('rb') as stream:
            magic = stream.read(8)
        if magic == b'!<arch>\n':
            raise ValueError(f'Unexpected archive in package; member audit required: {path}')
        if magic.startswith(b'\x7fELF'):
            entry = elf_requirement(path)
            if path.name == 'mainuan-welcome' and not entry['declared']:
                raise ValueError('Main executable lacks an explicit ISA requirement note')
            result.append(entry)
    if not result:
        raise ValueError('No distributed ELF files found')
    return result


def audit_flags(path):
    commands = json.loads(Path(path).read_text())
    if not commands:
        raise ValueError('Empty compilation database')
    forbidden = re.compile(r'^-m(?:avx|sse3|ssse3|sse4|fma|f16c|bmi|lzcnt|popcnt|movbe|cx16)')
    def check(args, label):
        if '-march=x86-64' not in args:
            raise ValueError(f'No explicit baseline target: {label}')
        for arg in args:
            if forbidden.match(arg) or (arg.startswith('-march=') and arg != '-march=x86-64') or arg.startswith('-mcpu=') or arg == '-mtune=native':
                raise ValueError(f'Incompatible compiler flag {arg}: {label}')
    for entry in commands:
        check(entry.get('arguments') or shlex.split(entry['command']), entry['file'])
    linked = 0
    for command_file in (Path(path).parent / 'CMakeFiles').rglob('link.txt'):
        for line in command_file.read_text().splitlines():
            args = shlex.split(line)
            if args and re.fullmatch(r'(?:c\+\+|g\+\+|gcc|clang\+\+)(?:-[0-9]+)?', Path(args[0]).name):
                check(args, str(command_file))
                linked += 1
    if not linked:
        raise ValueError('No CMake compiler link commands found (use Unix Makefiles)')
    return {'baseline_compile_commands': len(commands), 'baseline_link_commands': linked}


def dependencies(path):
    output = subprocess.check_output(['ldd', str(path.resolve())], text=True, env={**os.environ, 'LC_ALL': 'C'})
    if 'not found' in output:
        raise ValueError('Unresolved runtime dependencies: ' + output)
    paths = sorted(set(re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)', output, re.M)))
    if not paths:
        raise ValueError('No resolved runtime dependencies')
    return {'ldd': output, 'libraries': [elf_requirement(p) for p in paths]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=('environment', 'elf', 'package', 'flags', 'dependencies'))
    parser.add_argument('path', nargs='?', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.kind != 'environment' and args.path is None:
        parser.error('path is required for this audit')
    try:
        if args.kind == 'environment':
            report = environment()
        elif args.kind == 'flags':
            report = audit_flags(args.path)
        elif args.kind == 'dependencies':
            if args.path.suffix == '.deb':
                with tempfile.TemporaryDirectory() as folder:
                    subprocess.run(['dpkg-deb', '-x', str(args.path.resolve()), folder], check=True)
                    report = dependencies(Path(folder) / 'usr/bin/mainuan-welcome')
            else:
                report = dependencies(args.path)
        elif args.kind == 'elf':
            report = elf_requirement(args.path)
            if report is None:
                raise ValueError('Not an ELF file')
        else:
            with tempfile.TemporaryDirectory() as folder:
                subprocess.run(['dpkg-deb', '-x', str(args.path.resolve()), folder], check=True)
                report = audit_tree(folder)
                for entry in report:
                    entry['path'] = str(Path(entry['path']).relative_to(folder))
        output = json.dumps(report, indent=2)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output + '\n')
        print(output)
    except (OSError, ValueError, struct.error, subprocess.SubprocessError) as error:
        parser.exit(1, f'CPU compatibility validation failed: {error}\n')


if __name__ == '__main__':
    main()
