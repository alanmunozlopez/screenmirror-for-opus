"""Actual controller and sharp engine, independently checked with ASan/UBSan."""
from pathlib import Path
import os
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ModesIndependent(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = ROOT / 'build/modes-independent'
        directory.mkdir(parents=True, exist_ok=True)
        cls.binary = directory / 'controller-tests'
        subprocess.run([
            'cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pedantic',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
            '-DSCREENMIRROR_TEST_INTERLEAVING=1', '-DSCREENMIRROR_TEST_PREPARE_COST=1',
            '-include', str(ROOT / 'tests/ModesTestHooks.h'),
            '-I' + os.environ.get('SCREENMIRROR_TEST_SDK', str(ROOT / 'tests/sdk-stubs')),
            str(ROOT / 'tests/ModesControllerTests.c'),
            str(ROOT / 'src/modes/plugin/rgb_sender.c'),
            '-o', str(cls.binary),
        ], check=True)

    def test_settings_capture_and_transport(self):
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
        for test in range(1, 11):
            for persisted in ((0, 1, 2, 3, -1, 4) if test == 1 else (0, 3)):
                with self.subTest(test=test, persisted=persisted):
                    subprocess.run([str(self.binary), str(test), str(persisted)], check=True, env=environment)


if __name__ == '__main__':
    unittest.main()
