includes("volt-ui")

target("code-yoyo")
    set_kind("binary")
    set_languages("c++17")
    add_deps("volt-ui")
    add_packages("libsdl3", "imgui")
    add_files("src/**.cpp")
    add_includedirs("src", {public = true})
    add_defines("LOG_USE_COLOR", "IMGUI_ENABLE_DOCKING")

    if is_plat("windows") then
        add_syslinks("kernel32", "user32", "gdi32", "ole32")
    elseif is_plat("linux") then
        add_syslinks("pthread", "util")
    elseif is_plat("macosx") then
        add_syslinks("pthread")
    end

    after_build(function (target)
        os.cp("fonts", target:targetdir())
    end)

target_end()