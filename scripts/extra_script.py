import os
import sys
from pathlib import Path
Import("env")

sys.path.insert(0, env.subst("$PROJECT_DIR"))
from scripts.build_ca_store import build_ca_store

provisioning = Path(env.subst("$PROJECT_DATA_DIR")) / "provisioning"
build_ca_store(provisioning / "ca.crt", provisioning / "certs.ar")

# Includi anche i path del toolchain (molto importante per ESP32, AVR, ecc.)
env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)

# Metti compile_commands.json nella root del progetto
env.Replace(COMPILATIONDB_PATH=os.path.join("$PROJECT_DIR", "compile_commands.json"))

