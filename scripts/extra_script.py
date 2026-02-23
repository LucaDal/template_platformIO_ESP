import os
Import("env")

# Includi anche i path del toolchain (molto importante per ESP32, AVR, ecc.)
env.Replace(COMPILATIONDB_INCLUDE_TOOLCHAIN=True)

# Metti compile_commands.json nella root del progetto
env.Replace(COMPILATIONDB_PATH=os.path.join("$PROJECT_DIR", "compile_commands.json"))

