#!/usr/bin/env python3
"""Start the packaged UI under a verified baseline QEMU CPU, offscreen."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

CPU = 'qemu64,-sse3,-cx16,-lahf-lm'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--qemu', default='qemu-x86_64')
    args = parser.parse_args()
    qemu = shutil.which(args.qemu)
    if not qemu:
        parser.error('qemu-x86_64 is mandatory for the CPU compatibility gate')
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        probe = root / 'cpu-probe'
        subprocess.run(['cc', '-march=x86-64', '-mtune=generic', str(Path(__file__).with_name('cpu_probe.c')), '-o', str(probe)], check=True)
        subprocess.run([qemu, '-cpu', CPU, str(probe)], check=True)
        payload = root / 'package'
        subprocess.run(['dpkg-deb', '-x', str(args.package.resolve()), str(payload)], check=True)
        env = os.environ.copy()
        for key in ('DBUS_SESSION_BUS_ADDRESS', 'DISPLAY', 'WAYLAND_DISPLAY', 'QT_LOGGING_RULES', 'LD_PRELOAD', 'LD_LIBRARY_PATH'):
            env.pop(key, None)
        for key, folder in (('HOME', 'home'), ('XDG_CONFIG_HOME', 'config'), ('XDG_DATA_HOME', 'data'), ('XDG_CACHE_HOME', 'cache'), ('XDG_RUNTIME_DIR', 'runtime')):
            path = root / folder
            path.mkdir(mode=0o700)
            env[key] = str(path)
        env.update(QT_QPA_PLATFORM='offscreen', QT_QUICK_BACKEND='software', QSG_INFO='1')
        command = [qemu, '-cpu', CPU, str(payload / 'usr/bin/mainuan-welcome')]
        # LD_DEBUG demonstrates that the Qt Quick root was actually loaded. A
        # sleeping/blocked process alone is not evidence of successful startup.
        env['LD_DEBUG'] = 'libs'
        try:
            result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        except subprocess.TimeoutExpired as error:
            output = (error.stdout or b'').decode(errors='replace')
            bad = ('CPU ISA level is lower', 'Illegal instruction', 'Não foi possível carregar',
                   'QQmlApplicationEngine failed', 'is not installed', 'Cannot override')
            loaded = all(re.search(r'calling init:\s+\S*' + re.escape(library), output) for library in ('libQt6Quick.so.6', 'libqtquickcontrols2plugin.so', 'libKirigamiplugin.so'))
            if any(text in output for text in bad) or not loaded or 'Loading backend software' not in output:
                parser.exit(1, 'Baseline UI startup not confirmed:\n' + output[-24000:] + '\n')
            print('Packaged UI stayed active for 30s with Qt Quick/Controls plugins loaded under ' + CPU, flush=True)
            print(output, flush=True)
        else:
            parser.exit(1, f'Baseline UI exited unexpectedly ({result.returncode}):\n{result.stdout.decode(errors="replace")}\n')


if __name__ == '__main__':
    main()
