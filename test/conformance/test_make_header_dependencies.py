import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class HeaderDependencies(unittest.TestCase):
    def test_detail_header_edit_rebuilds_existing_host_object(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shutil.copyfile(ROOT / 'Makefile', root / 'Makefile')
            shutil.copytree(ROOT / 'mk', root / 'mk')
            inputs = ['test/test_integers.cpp', 'include/ffmt/base.hpp',
                      'include/ffmt/detail/dependency_probe.hpp',
                      'fmt/include/fmt/format.h', 'build/.dir', 'build/.flags']
            for name in inputs:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('// dependency fixture\n')
                os.utime(path, (1000000000, 1000000000))
            target = root / 'build/test_main_host_specifiers_integers.o'
            target.touch()
            os.utime(target, (1000000200, 1000000200))
            command = ['make', '-n', '-o', 'build/.flags', str(target.relative_to(root)),
                       'CXX=c++', 'FMT_DIR=fmt']
            before = subprocess.check_output(command, cwd=root, text=True)
            self.assertNotIn('-c test/test_integers.cpp', before)
            os.utime(root / 'include/ffmt/detail/dependency_probe.hpp',
                     (1000000300, 1000000300))
            after = subprocess.check_output(command, cwd=root, text=True)
            self.assertIn('-c test/test_integers.cpp', after)


if __name__ == '__main__':
    unittest.main()
