import json
from pathlib import Path
import struct
import tempfile
import unittest

import check_cpu


def elf_note(mask, property_type=check_cpu.ISA_NEEDED):
    prop = struct.pack('<III4x', property_type, 4, mask)
    note = struct.pack('<III', 4, len(prop), 5) + b'GNU\0' + prop
    header = bytearray(64)
    header[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<H', header, 18, 62)
    struct.pack_into('<Q', header, 32, 64)
    struct.pack_into('<HHHH', header, 54, 56, 1, 64, 0)
    program = struct.pack('<IIQQQQQQ', 4, 0, 120, 0, 0, len(note), len(note), 8)
    return header + program + note


class CpuGates(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'binary'

    def test_baseline_and_optional_used_features(self):
        for mask, kind in [(1, check_cpu.ISA_NEEDED), (7, 0xC0010002), (3, 0xC0000002)]:
            with self.subTest(mask=mask, kind=kind):
                self.path.write_bytes(elf_note(mask, kind))
                result = check_cpu.elf_requirement(self.path)
                self.assertLessEqual(result['needed_mask'], 1)

    def test_higher_or_unknown_required_isa_is_rejected(self):
        for mask in (2, 7, 15, 0x100):
            with self.subTest(mask=mask), self.assertRaises(ValueError):
                self.path.write_bytes(elf_note(mask))
                check_cpu.elf_requirement(self.path)

    def test_truncated_or_wrong_architecture(self):
        for data in (elf_note(1)[:-1], b'\x7fELF', b'\x7fELF\x01\x01' + bytes(60)):
            with self.subTest(data=data), self.assertRaises(ValueError):
                self.path.write_bytes(data)
                check_cpu.elf_requirement(self.path)

    def test_debian_amd64_can_still_be_a_variant(self):
        self.assertEqual(check_cpu.installed_variants('qt:amd64\tamd64\t\t6.10\n'), [])
        self.assertTrue(check_cpu.installed_variants('qt:amd64\tamd64\tamd64v3\t6.10\n'))
        self.assertTrue(check_cpu.installed_variants('truncated\n'))

    def test_every_distributed_elf_is_audited(self):
        self.path.write_text('script')
        extra = self.path.parent / 'library.so'
        extra.write_bytes(elf_note(7))
        with self.assertRaises(ValueError):
            check_cpu.audit_tree(self.path.parent)

    def test_flags_reject_native_and_unconditional_extensions(self):
        link = self.path.parent / 'CMakeFiles/app.dir/link.txt'
        link.parent.mkdir(parents=True)
        link.write_text('c++ -march=x86-64 -mtune=generic app.o -o app\n')
        for flag in ('-march=native', '-march=x86-64-v3', '-mavx2', '-msse4.2', '-mbmi2'):
            self.path.write_text(json.dumps([{'file': 'test.cpp', 'arguments': ['c++', '-march=x86-64', flag]}]))
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                check_cpu.audit_flags(self.path)
        self.path.write_text(json.dumps([{'file': 'test.cpp', 'arguments': ['c++', '-march=x86-64', '-mtune=generic']}]))
        self.assertEqual(check_cpu.audit_flags(self.path)['baseline_compile_commands'], 1)

    def test_lto_link_flags_cannot_raise_the_target(self):
        self.path.write_text(json.dumps([{'file': 'test.cpp', 'arguments': ['c++', '-march=x86-64']}]))
        link = self.path.parent / 'CMakeFiles/app.dir/link.txt'
        link.parent.mkdir(parents=True)
        link.write_text('c++ -march=x86-64 -flto -march=native app.o -o app\n')
        with self.assertRaises(ValueError):
            check_cpu.audit_flags(self.path)


if __name__ == '__main__':
    unittest.main()
