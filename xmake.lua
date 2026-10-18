-- include subprojects
includes("lib/commonlibf4")

-- set project constants
set_project("4Stim")
set_version("0.3.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- third-party packages (fetched by xmake on first build)
add_requires("nlohmann_json")
add_requires("zydis")  -- CommonLibF4RD's resolver decodes the game's code (src/compat/rd/)

-- CommonLibF4's "commonlibf4.plugin" rule, with 4Stim's own F4SE version data
-- (src/PluginVersion.cpp: address-independent, both struct layouts) instead
-- of the generated one, which names exact game versions: 4Stim runs on any
-- version CommonLibF4RD's Runtime Database covers (docs/OLDGEN.md).
rule("4stim.plugin", function()
    add_deps("commonlib.plugin")

    on_load(function(target)
        target:data_set("commonlib.plugin.config", target:extraconf("rules", "4stim.plugin"))
        target:data_set("commonlib.plugin.package", { prefixdir = "Data" })
    end)

    on_config(function(target)
        import("core.base.semver")
        target:add("deps", "commonlibf4")
        local v = semver.new(target:version() or "0.0.0")
        target:add("defines", "FOURSTIM_VERSION_MAJOR=" .. v:major(), "FOURSTIM_VERSION_MINOR=" .. v:minor(), "FOURSTIM_VERSION_PATCH=" .. v:patch())
        target:add("installfiles", target:targetfile(), { prefixdir = "F4SE/Plugins" })
        target:add("installfiles", target:symbolfile(), { prefixdir = "F4SE/Plugins" })
    end)
end)

-- define targets
target("4Stim")
    add_rules("4stim.plugin", {
        name = "4Stim",
        author = "KeryoWolfe",
        description = "A scene/animation framework for Fallout 4, inspired by OStim"
    })

    add_packages("nlohmann_json", "zydis")
    add_syslinks("version")  -- the game's file version (src/compat/rd/RDSupport.cpp)
    add_syslinks("winhttp")  -- Intiface Central's WebSocket (src/Intiface.cpp)

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "include")
    add_headerfiles("include/**.h")
    set_pcxxheader("src/pch.h")
