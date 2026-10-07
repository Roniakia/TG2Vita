import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('config_install', Path(__file__).resolve().parents[1] / 'scripts/install-config.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ProvisioningTests(unittest.TestCase):
    def test_private_copy_and_existing_file_preservation(self):
        first = b'api_id=12345\napi_hash=' + b'a' * 32 + b'\nuse_test_dc=true\n'
        second = first.replace(b'12345', b'12346')
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()):
            root = Path(directory)
            module.install(first, root)
            self.assertEqual((root / 'telegram.conf').read_bytes(), first)
            self.assertEqual((root / 'telegram.conf').stat().st_mode & 0o777, 0o600)
            module.install(first, root)
            with self.assertRaises(ValueError):
                module.install(second, root)
            self.assertEqual((root / 'telegram.conf').read_bytes(), first)
            with self.assertRaises(ValueError):
                module.install(b'api_id=0\napi_hash=\n', root, replace=True)
            self.assertEqual((root / 'telegram.conf').read_bytes(), first)
            module.install(second, root, replace=True)
            self.assertEqual((root / 'telegram.conf').read_bytes(), second)
            self.assertEqual(list(root.glob('.telegram-conf-*')), [])

    def test_secret_free_validation_errors(self):
        with self.assertRaisesRegex(ValueError, 'Duplicate configuration setting'):
            module.validate(b'api_hash=' + b'a' * 32 + b'\napi_hash=' + b'b' * 32)
        with self.assertRaisesRegex(ValueError, 'Invalid use_test_dc'):
            module.validate(b'api_id=12345\napi_hash=' + b'a' * 32 + b'\nuse_test_dc=bad')


if __name__ == '__main__':
    unittest.main()
