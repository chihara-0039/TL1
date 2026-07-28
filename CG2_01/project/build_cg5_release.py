"""CG5評価確認用の実行ファイル一式を、再配布可能なZIPへまとめる。"""

from __future__ import annotations

from datetime import datetime
from pathlib import Path
import shutil
from zipfile import ZIP_DEFLATED, ZipFile


PROJECT_DIR = Path(__file__).resolve().parent
REPO_DIR = PROJECT_DIR.parent
RELEASE_OUTPUT = REPO_DIR / "generated" / "outputs" / "Release"
ARTIFACTS_DIR = REPO_DIR / "artifacts"


def require(path: Path) -> Path:
    if not path.exists():
        raise FileNotFoundError(f"Required release file was not found: {path}")
    return path


def main() -> None:
    executable = require(RELEASE_OUTPUT / "CG2_01.exe")
    dxcompiler = require(RELEASE_OUTPUT / "dxcompiler.dll")
    dxil = require(RELEASE_OUTPUT / "dxil.dll")
    resources = require(PROJECT_DIR / "Resources")
    readme = require(PROJECT_DIR / "README.md")

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    package_name = f"CG5_Evaluation_Release_{timestamp}"
    package_dir = ARTIFACTS_DIR / package_name
    runtime_dir = package_dir / "Runtime"
    runtime_dir.mkdir(parents=True, exist_ok=False)

    for file_path in (executable, dxcompiler, dxil):
        shutil.copy2(file_path, runtime_dir / file_path.name)
    shutil.copytree(resources, runtime_dir / "Resources")
    shutil.copy2(readme, package_dir / "README.md")

    build_info = package_dir / "BUILD_INFO.txt"
    build_info.write_text(
        "\n".join(
            [
                "CG5 Evaluation Task 1 - Release Package",
                "Configuration: Release | x64",
                "Executable: Runtime/CG2_01.exe",
                "Working directory: Runtime",
                "Gameplay controls: N=Normal, 1=Grayscale, 2-0=Extra PostEffects",
                f"Built executable timestamp: {datetime.fromtimestamp(executable.stat().st_mtime)}",
            ]
        )
        + "\n",
        encoding="utf-8",
    )

    zip_path = ARTIFACTS_DIR / f"{package_name}.zip"
    with ZipFile(zip_path, "w", ZIP_DEFLATED, allowZip64=True) as archive:
        for path in package_dir.rglob("*"):
            if path.is_file():
                archive.write(path, Path(package_name) / path.relative_to(package_dir))

    print(package_dir)
    print(zip_path)


if __name__ == "__main__":
    main()
