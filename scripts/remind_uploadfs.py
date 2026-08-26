Import("env")

# Firmware upload does not flash LittleFS. quotes.bin has to be sent separately.

def _remind(source, target, env):
    pioenv = env.subst("$PIOENV")
    if pioenv in ("hello", "repair"):
        return
    print()
    print("*** Quotes are stored in LittleFS, not in the firmware image.")
    print("*** If the panel has no verses, also run:")
    print("***   pio run -e %s -t uploadfs" % pioenv)
    print()


env.AddPostAction("upload", env.VerboseAction(_remind, "Filesystem upload reminder"))
