"""Firmware CI installer policy: PR preflight cannot authorize a physical flash."""
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class FlashInstallerContract(unittest.TestCase):
    def test_verify_only_can_validate_explicit_pr_run_without_flashing(self):
        installer = (ROOT / "scripts/flash-from-github.ps1").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/verify-active-sources.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "if (-not $VerifyOnly -and ($run.headBranch -ne $branch -or $run.status -ne 'completed'))",
            installer,
        )
        self.assertIn("if (-not $VerifyOnly) {", installer)
        self.assertIn(
            "if (-not $VerifyOnly -and $run.conclusion -ne 'success' -and -not $BenchCandidate)",
            installer,
        )
        self.assertIn("foreach ($name in $required)", installer)
        self.assertIn("if ($VerifyOnly) {", installer)
        self.assertLess(
            installer.index("if ($VerifyOnly) {"),
            installer.index("& python -m esptool"),
        )
        self.assertIn("-RunId ${{ github.run_id }}", workflow)
        self.assertIn("-VerifyOnly", workflow)


if __name__ == "__main__":
    unittest.main()
