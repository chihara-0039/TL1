from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


SOURCE = Path(r"C:\Users\k024g\OneDrive\デスクトップ\LE3C_15_チハラ_シゴウ")
DEST = Path(r"C:\Users\k024g\OneDrive\デスクトップ\LE3C_15_チハラ_シゴウ.zip")


def should_skip(path: Path) -> bool:
    rel_parts = path.relative_to(SOURCE).parts
    if ".vs" in rel_parts:
        return True
    if path.suffix.lower() in {".ipch", ".suo", ".db", ".vsidx"}:
        return True
    return False


def main() -> None:
    with ZipFile(DEST, "w", ZIP_DEFLATED, allowZip64=True) as zf:
        for path in SOURCE.rglob("*"):
            if path.is_dir() or should_skip(path):
                continue
            arcname = Path(SOURCE.name) / path.relative_to(SOURCE)
            zf.write(path, arcname.as_posix())
    print(DEST)


if __name__ == "__main__":
    main()
