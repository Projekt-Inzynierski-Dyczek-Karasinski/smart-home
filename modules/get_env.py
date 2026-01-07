Import("env")

from os.path import isfile, join

env_path = join(env.subst("$PROJECT_DIR"), ".env")
if not isfile(env_path):
    raise FileNotFoundError(f"Missing required .env file.")

with open(".env", "r", encoding="utf-8") as f:
    flags = []
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        print(f"Creating macro from .env file: {line}")
        name, value = line.split("=")
        result = f'-D{name}=\\{value[:-1]}\\"'
        flags.append(result)

env.Append(BUILD_FLAGS=flags)
