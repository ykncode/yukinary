"""End-to-end acceptance test for the local Yukinary package workflow."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


PACKAGE_SOURCE = """isv sayac() -> tamsayi {
    dondu 42;
}
"""


MAIN_SOURCE = """isv ana() -> tamsayi {
    eger sayac() == 42 ise {
        ekrana_yaz(\"PACKAGE-OK\\n\".ptr);
    }
    dondu 0;
}
"""


def run_launcher(launcher: Path, project: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(launcher), *arguments],
        cwd=project,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        timeout=180,
        check=False,
    )


def manifest_value(manifest: str, key: str) -> str:
    match = re.search(rf"(?m)^{re.escape(key)}\s*=\s*\"([^\"]*)\"\s*$", manifest)
    if match is None:
        raise AssertionError(f"manifest key not found: {key}")
    return match.group(1)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: 01_package_manager.py <package-launcher>", file=sys.stderr)
        return 2

    launcher = Path(sys.argv[1]).resolve()
    if not launcher.is_file():
        print(f"package launcher not found: {launcher}", file=sys.stderr)
        return 2

    compiler = launcher.parent / "app_full.exe"
    if not compiler.is_file():
        print(f"compiler beside launcher not found: {compiler}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="yukinary_package_") as tmp:
        project = Path(tmp)
        package = project / "pkg" / "sayac"
        package.mkdir(parents=True)
        (package / "mod.ykn").write_text(PACKAGE_SOURCE, encoding="utf-8", newline="\n")
        (project / "main.ykn").write_text(MAIN_SOURCE, encoding="utf-8", newline="\n")

        added = run_launcher(launcher, project, "add", "sayac")
        if added.returncode != 0:
            print(added.stdout + added.stderr, file=sys.stderr)
            return 1

        manifest_path = project / "yukinary.toml"
        if not manifest_path.is_file():
            print("yukinary.toml was not created", file=sys.stderr)
            return 1
        manifest = manifest_path.read_text(encoding="utf-8")
        if manifest_value(manifest, "sayac") != "pkg/sayac":
            print(f"unexpected dependency entry:\n{manifest}", file=sys.stderr)
            return 1
        if manifest_value(manifest, "main") != "main.ykn":
            print(f"unexpected main entry:\n{manifest}", file=sys.stderr)
            return 1

        # Re-adding the same local package must be harmless and must not
        # duplicate the dependency entry.
        added_again = run_launcher(launcher, project, "add", "sayac")
        if added_again.returncode != 0 or manifest_path.read_text(encoding="utf-8").count('sayac = "pkg/sayac"') != 1:
            print(added_again.stdout + added_again.stderr, file=sys.stderr)
            return 1

        built = run_launcher(launcher, project, "build")
        if built.returncode != 0:
            print(built.stdout + built.stderr, file=sys.stderr)
            return 1

        output_relative = manifest_value(manifest, "output")
        output = project / output_relative
        if not output.is_file():
            print(f"build output was not created: {output}", file=sys.stderr)
            return 1

        executed = subprocess.run(
            [str(output)],
            cwd=project,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=30,
            check=False,
        )
        output_text = executed.stdout + executed.stderr
        if executed.returncode != 0 or "PACKAGE-OK" not in output_text:
            print(output_text, file=sys.stderr)
            return 1

    print("PACKAGE-MANAGER-PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
