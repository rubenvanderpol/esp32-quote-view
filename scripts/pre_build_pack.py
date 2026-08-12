Import("env")

import os
import subprocess
import sys
from pathlib import Path

project_dir = Path(env["PROJECT_DIR"])
tool_src = project_dir / "tools" / "pack_quotes.cpp"
tool_bin = project_dir / "tools" / "pack_quotes"

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

subprocess.check_call([str(tool_bin)], cwd=str(project_dir))
