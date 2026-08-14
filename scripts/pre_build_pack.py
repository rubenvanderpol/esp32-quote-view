Import("env")

import subprocess
import sys
from pathlib import Path

project_dir = Path(env["PROJECT_DIR"])
tool_src = project_dir / "tools" / "pack_quotes.cpp"
tool_bin = project_dir / "tools" / "pack_quotes"
quotes_json = project_dir / "data" / "quotes.json"
quotes_bin = project_dir / "fs" / "quotes.bin"

subprocess.check_call(
    [
        "g++",
        "-std=c++17",
        "-O2",
        "-Wall",
        "-Wextra",
        str(tool_src),
        "-o",
        str(tool_bin),
    ]
)

subprocess.check_call(
    [str(tool_bin), str(quotes_json), str(quotes_bin)], cwd=str(project_dir)
)

# include/lora.h is committed; regenerate it only when missing so ordinary
# builds don't require freetype-py. Run scripts/fontconvert_lora.py manually
# after changing the font or size.
lora_header = project_dir / "include" / "lora.h"
if not lora_header.exists():
    subprocess.check_call(
        [sys.executable, str(project_dir / "scripts" / "fontconvert_lora.py")]
    )
