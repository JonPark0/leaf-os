# Lua 5.4.8 core (https://www.lua.org/) — MIT licence, see LICENSE.
# io / os / package / debug libraries are deliberately NOT built: SD apps are
# sandboxed and reach the host only through the bindings in main/apps/lua/.
idf_component_register(
    SRCS
        "src/lapi.c" "src/lcode.c" "src/lctype.c" "src/ldebug.c" "src/ldo.c"
        "src/ldump.c" "src/lfunc.c" "src/lgc.c" "src/llex.c" "src/lmem.c"
        "src/lobject.c" "src/lopcodes.c" "src/lparser.c" "src/lstate.c"
        "src/lstring.c" "src/ltable.c" "src/ltm.c" "src/lundump.c" "src/lvm.c"
        "src/lzio.c"
        "src/lauxlib.c" "src/lbaselib.c" "src/lcorolib.c" "src/lmathlib.c"
        "src/lstrlib.c" "src/ltablib.c" "src/lutf8lib.c"
    INCLUDE_DIRS "src"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -w)
