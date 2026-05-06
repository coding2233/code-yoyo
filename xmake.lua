includes("volt-ui")

target("CodeYoYo")
    set_kind("binary")
    set_languages("c++17")
    add_deps("volt-ui")
    add_packages("libsdl3", "imgui")
    add_files("src/**.cpp")
    add_includedirs("src", {public = true})
