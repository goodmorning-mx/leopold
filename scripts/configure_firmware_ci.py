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
config_header = config_dir / "leopold_config.h"
if not config_header.exists() or config_header.read_text() != config:
    config_header.write_text(config)

commit = subprocess.check_output(
    ["git", "rev-parse", "HEAD"], cwd=project, text=True
).strip()
build_id = sha256(f"{commit}\n{config}".encode()).hexdigest()[:16]
env.Prepend(CPPPATH=[str(config_dir)])
identity = (
    "#pragma once\n"
    f'#define LEOPOLD_SOURCE_COMMIT "{commit}"\n'
    f'#define LEOPOLD_BUILD_ID "{build_id}"\n'
)
identity_header = config_dir / "leopold_build_identity.h"
if not identity_header.exists() or identity_header.read_text() != identity:
    identity_header.write_text(identity)
