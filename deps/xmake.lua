target("reproc")
    set_kind("static")
    add_includedirs("reproc/reproc/include", {public = true})
    add_defines("_CRT_NONSTDC_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS")
    if is_plat("windows") then
        add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
        add_files("reproc/reproc/src/*.c|*.posix.c")
        add_syslinks("ws2_32")
    else
        add_files("reproc/reproc/src/*.c|*.windows.c")
    end

target("reproc++")
    set_kind("static")
    add_deps("reproc")
    add_includedirs("reproc/reproc++/include", {public = true})
    add_defines("_CRT_NONSTDC_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS")
    if is_plat("windows") then
        add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
    end
    add_files("reproc/reproc++/src/*.cpp")
