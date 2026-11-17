"""Supply synthetic device configuration for the portable compile."""
from hashlib import sha256
from pathlib import Path
import subprocess

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
config_dir = Path(env.subst("$PROJECT_BUILD_DIR")) / "config"
config_dir.mkdir(parents=True, exist_ok=True)
config = (project / "include" / "leopold_config.example.h").read_text()
config = config.replace('"LPD-CHANGE-ME"', '"LPD-CI-COMPILE"')
config = config.replace('"CHANGE-ME"', '"ci-compile-placeholder"')
(config_dir / "leopold_config.h").write_text(config)

commit = subprocess.check_output(
    ["git", "rev-parse", "HEAD"], cwd=project, text=True
).strip()
build_id = sha256(f"{commit}\n{config}".encode()).hexdigest()[:16]
env.Prepend(CPPPATH=[str(config_dir)])
env.Append(CPPDEFINES=[
    ("LEOPOLD_SOURCE_COMMIT", f'\\"{commit}\\"'),
    ("LEOPOLD_BUILD_ID", f'\\"{build_id}\\"'),
])
