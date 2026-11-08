-- include subprojects
includes("lib/commonlibf4")

-- set project constants
set_project("4Stim")
set_version("0.5.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
-- releasedbg unless `xmake f -m` says otherwise: optimized, with a .pdb
-- (plain "release" isn't one of the modes here and builds without one).
set_defaultmode("releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- third-party packages (fetched by xmake on first build)
add_requires("nlohmann_json")
add_requires("yaml-cpp")  -- YAML scene files (src/YamlJSON.cpp)
add_requires("zydis")  -- CommonLibF4RD's resolver decodes the game's code (src/compat/rd/)

-- CommonLibF4's "commonlibf4.plugin" rule, with 4Stim's own F4SE version data
-- (src/PluginVersion.cpp: address-independent, both struct layouts) instead
-- of the generated one, which names exact game versions: 4Stim runs on any
-- version CommonLibF4RD's Runtime Database covers (docs/OLDGEN.md).
rule("4stim.plugin", function()
    add_deps("commonlib.plugin")

    on_load(function(target)
        -- Fixes to Dear Modding's CommonLibF4 (c71da99), made to the
        -- submodule's headers before anything compiles, until the fork has
        -- them. Each is { file, what's wrong, what it should be }; a file
        -- that already has the fix is left alone, and one with neither stops
        -- the build (the fork changed: look again).
        --  1. RE::Actor has no TYPE_ID, so it inherits TESObjectREFR's
        --     (ObjectReference): natives taking an Actor were registered as
        --     taking an ObjectReference and refused to bind ("does not match
        --     existing signature"). kActor is 65, the old FORM_ID kACHR.
        --  2. AIProcess::SetupSpecialIdle and StopCurrentIdle are one ID
        --     too high for NG/AE (2231705 and 2231706; libxse's CommonLibF4,
        --     which 4Stim played scenes with on 1.11.240, has 2231704 and
        --     2231705, as 4Stim's own compat/RequiredIDs.h does). With them,
        --     PlayIdle called StopCurrentIdle and every idle was refused.
        --  3. A None array passed to a native taking an array (a Papyrus
        --     "string[] empty" never given a value) unpacks through a null
        --     pointer and crashes the game (UnpackVariable for arrays): the
        --     owner's crash calling FourLibrary.GetRandomScene. It becomes an
        --     empty array, as Papyrus treats a None array (Length 0).
        local fixes = {
            { "include/RE/A/Actor.h",
              "static constexpr auto FORM_ID{ ENUM_FORM_ID::kACHR };",
              "static constexpr auto FORM_ID{ ENUM_FORM_ID::kACHR }; static constexpr auto TYPE_ID{ BSScript::kActor };" },
            { "include/RE/IDs.h",
              "SetupSpecialIdle{ 1446774, 2231705 }",
              "SetupSpecialIdle{ 1446774, 2231704 }" },
            { "include/RE/IDs.h",
              "StopCurrentIdle{ 434460, 2231706 }",
              "StopCurrentIdle{ 434460, 2231705 }" },
            { "include/RE/B/BSScriptUtil.h",
              "const auto in = get<Array>(a_var);",
              "const auto in = get<Array>(a_var); if (!in) { return out; }  // 4Stim: a None array is an empty one" },
        }
        for _, fix in ipairs(fixes) do
            local file = path.join(os.projectdir(), "lib/commonlibf4", fix[1])
            if os.isfile(file) then
                local text = io.readfile(file)
                -- (Actor.h fixed by an earlier 4Stim build has TYPE_ID on a line of its own)
                local done = text:find(fix[3], 1, true) or (fix[1]:find("Actor.h", 1, true) and text:find("TYPE_ID{ BSScript::kActor }", 1, true))
                    -- (KeryoWolfe/CommonLibF4's 4stim/combined has all three fixes, worded its own way)
                    or (fix[1]:find("BSScriptUtil.h", 1, true) and text:find("a None array (a Papyrus array never given a value) is an empty one", 1, true))
                if not done then
                    local s, e = text:find(fix[2], 1, true)
                    if not s then
                        raise("4Stim: can't fix CommonLibF4's " .. fix[1] .. ": neither \"" .. fix[2] .. "\" nor the fix is in it")
                    end
                    io.writefile(file, text:sub(1, s - 1) .. fix[3] .. text:sub(e + 1))
                    print("4Stim: fixed CommonLibF4's " .. fix[1] .. " (" .. fix[3]:gsub("%s+", " ") .. ")")
                end
            end
        end

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

    add_packages("nlohmann_json", "yaml-cpp", "zydis")
    add_syslinks("version")  -- the game's file version (src/compat/rd/RDSupport.cpp)
    add_syslinks("winhttp")  -- Intiface Central's WebSocket (src/Intiface.cpp)

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "include")
    add_defines("FOURSTIM_API_IMPLEMENTATION")  -- include/4Stim headers without their loaders (Windows.h)
    add_headerfiles("include/**.h")
    set_pcxxheader("src/pch.h")

    -- After each build, copy the DLL (and its .pdb) into the 4Stim Core mod,
    -- so the game picks up the new build without copying it by hand. The
    -- folder is the owner's; set FOURSTIM_DEPLOY to use another (or to an
    -- empty string to turn this off). Skipped when the folder isn't there.
    after_build(function(target)
        local dest = os.getenv("FOURSTIM_DEPLOY")
        if dest == nil then
            dest = "D:/Naked Commonwealth/mods/4Stim Core/F4SE/Plugins"
        end
        if dest == "" or not os.isdir(dest) then
            return
        end
        for _, file in ipairs({ target:targetfile(), target:symbolfile() }) do
            if file and os.isfile(file) then
                try {
                    function()
                        os.cp(file, dest)
                        cprint("${green}deployed${clear} %s -> %s", path.filename(file), dest)
                    end,
                    catch {
                        function(e)
                            cprint("${yellow}not deployed${clear} %s (is the game running?): %s", path.filename(file), tostring(e))
                        end
                    }
                }
            end
        end
    end)
