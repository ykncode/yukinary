import tempfile
import unittest
from pathlib import Path

from tools.audit_reality import scan


class RealityAuditTests(unittest.TestCase):
    def test_production_constant_success_requires_review(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "src" / "sample.ykn"
            source.parent.mkdir()
            source.write_text("isv unsupported() { dondu 0; }\n", encoding="utf-8")

            report = scan(root, ("src",))

            self.assertGreaterEqual(report["production_finding_count"], 1)
            self.assertTrue(any(item["class"] == "constant_success_body" for item in report["findings"]))
            self.assertEqual(report["claim_gate"], "REVIEW_REQUIRED")

    def test_reasoned_exact_class_annotation_is_auditable_suppression(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "std" / "sample.ykn"
            source.parent.mkdir()
            source.write_text(
                "dondu 0; # reality-audit: allow class=constant_success reason=zero means unsupported\n",
                encoding="utf-8",
            )

            report = scan(root, ("std",))

            self.assertEqual(report["production_finding_count"], 0)
            self.assertEqual(report["suppression_count"], 1)
            self.assertEqual(report["suppressions"][0]["reason"], "zero means unsupported")

    def test_multiline_empty_and_constant_success_functions_are_symbol_findings(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "std" / "sample.ykn"
            source.parent.mkdir()
            source.write_text(
                "isv empty() -> tamsayi {\n}\nisv unsupported() -> tamsayi {\n  dondu 0;\n}\nisv real() -> tamsayi {\n  dondu 4;\n}\n",
                encoding="utf-8",
            )

            report = scan(root, ("std",))
            semantic = [item for item in report["findings"] if item["class"] in {"empty_body", "constant_success_body"}]

            self.assertEqual([(item["symbol"], item["class"]) for item in semantic], [
                ("empty", "empty_body"),
                ("unsupported", "constant_success_body"),
            ])

    def test_symbol_finding_can_be_suppressed_only_with_exact_reasoned_annotation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "std" / "sample.ykn"
            source.parent.mkdir()
            source.write_text(
                "isv intentionally_empty() -> tamsayi { # reality-audit: allow class=empty_body reason=reserved ABI hook\n}\n",
                encoding="utf-8",
            )

            report = scan(root, ("std",))

            self.assertFalse(any(item["class"] == "empty_body" for item in report["findings"]))
            self.assertTrue(any(item["class"] == "empty_body" and item["symbol"] == "intentionally_empty" for item in report["suppressions"]))

    def test_unknown_annotation_class_is_not_suppressed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "src" / "sample.ykn"
            source.parent.mkdir()
            source.write_text(
                "dondu 0; # reality-audit: allow class=success reason=guarded error return\n",
                encoding="utf-8",
            )

            report = scan(root, ("src",))

            self.assertEqual(report["production_finding_count"], 2)
            self.assertEqual(report["findings"][0]["class"], "invalid_annotation")
            self.assertEqual(report["claim_gate"], "REVIEW_REQUIRED")


if __name__ == "__main__":
    unittest.main()
