from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT))

from tools import check_architecture


class ArchitectureCheckerTests(unittest.TestCase):
    def test_unknown_features_are_discovered_and_enforced(self) -> None:
        original_root = check_architecture.ROOT
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            check_architecture.ROOT = root
            try:
                alpha_header = root / "include" / "reach" / "features" / "alpha.h"
                beta_header = root / "include" / "reach" / "features" / "beta.h"
                alpha_source = root / "src" / "features" / "alpha" / "alpha.cpp"
                beta_source = root / "src" / "features" / "beta" / "beta.cpp"
                composition_source = root / "src" / "composition" / "host_probe.cpp"

                for path in (
                    alpha_header,
                    beta_header,
                    alpha_source,
                    beta_source,
                    composition_source,
                ):
                    path.parent.mkdir(parents=True, exist_ok=True)

                alpha_header.write_text("void reach_alpha_ping(void);\n", encoding="utf-8")
                beta_header.write_text("void reach_beta_ping(void);\n", encoding="utf-8")
                alpha_source.write_text(
                    '#include "reach/features/alpha.h"\n', encoding="utf-8"
                )
                beta_source.write_text(
                    '#include "reach/features/alpha.h"\n'
                    "void probe(void) { reach_alpha_ping(); }\n",
                    encoding="utf-8",
                )
                composition_source.write_text(
                    '#include "reach/features/beta.h"\n'
                    "void probe(void) { reach_beta_ping(); }\n",
                    encoding="utf-8",
                )

                headers = check_architecture.concrete_feature_headers()
                symbols = check_architecture.concrete_feature_symbols(headers)

                self.assertEqual(headers["reach/features/alpha.h"], "alpha")
                self.assertEqual(headers["reach/features/beta.h"], "beta")

                peer_violations = check_architecture.validate_feature_peer_dependencies(
                    beta_source, beta_source.read_text(encoding="utf-8"), headers, symbols
                )
                self.assertTrue(any("peer feature alpha" in item for item in peer_violations))
                self.assertTrue(any("reach_alpha_ping" in item for item in peer_violations))

                composition_violations = (
                    check_architecture.validate_composition_feature_boundary(
                        composition_source,
                        composition_source.read_text(encoding="utf-8"),
                        headers,
                        symbols,
                    )
                )
                self.assertTrue(
                    any("reach/features/beta.h" in item for item in composition_violations)
                )
                self.assertTrue(
                    any("reach_beta_ping" in item for item in composition_violations)
                )

                helper_source = root / "src" / "composition" / "host_helper_probe.cpp"
                helper_source.write_text(
                    "void reach_host_refresh_beta_windows(void) {}\n", encoding="utf-8"
                )
                pattern = check_architecture.composition_feature_helper_pattern(headers)
                helper_violations = check_architecture.validate_composition_feature_helpers(
                    helper_source, helper_source.read_text(encoding="utf-8"), pattern
                )
                self.assertTrue(
                    any(
                        "reach_host_refresh_beta_windows" in item
                        for item in helper_violations
                    )
                )
            finally:
                check_architecture.ROOT = original_root


if __name__ == "__main__":
    unittest.main()
