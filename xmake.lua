-- include subprojects
includes("lib/commonlibf4")

-- set project constants
set_project("4Stim")
set_version("0.0.1")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- third-party packages (fetched by xmake on first build)
add_requires("nlohmann_json")

-- define targets
target("4Stim")
    add_rules("commonlibf4.plugin", {
        name = "4Stim",
        author = "TODO: your name",
        description = "A scene/animation framework for Fallout 4, inspired by OStim"
    })

    add_packages("nlohmann_json")

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
