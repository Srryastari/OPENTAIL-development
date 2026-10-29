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

    def test_real_flash_must_match_current_head_not_a_stale_success(self):
        installer = (ROOT / "scripts/flash-from-github.ps1").read_text(encoding="utf-8")
        # A PR workflow run is commonly based on a synthetic merge SHA,
        # so the candidate head may be the run commit's second parent.
        begin = installer.index("# A completed run is not necessarily the current firmware candidate.")
        end = installer.index("# Stage the artifact", begin)
        guard = installer[begin:end]
        self.assertIn("if (-not $VerifyOnly) {", guard)
        self.assertIn('gh api "repos/$repo/git/ref/heads/$branch"', guard)
        self.assertIn('gh api "repos/$repo/commits/$($run.headSha)"', guard)
        self.assertIn("$run.headSha -ne $currentHead", guard)
        self.assertIn("$parents.Count -ne 2", guard)
        self.assertIn("$parents[1] -ne $currentHead", guard)
        self.assertIn("Nothing was flashed.", guard)
        self.assertLess(end, installer.index("gh run download"))
        self.assertLess(end, installer.index("& python -m esptool"))


if __name__ == "__main__":
    unittest.main()
