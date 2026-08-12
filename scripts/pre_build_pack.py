Import("env")

import subprocess
import sys
from pathlib import Path

script = Path(env["PROJECT_DIR"]) / "scripts" / "pack_quotes.py"
subprocess.check_call([sys.executable, str(script)])
