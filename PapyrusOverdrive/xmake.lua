-- Papyrus Overdrive: a separate F4SE plugin built alongside 4Stim (the root
-- xmake.lua includes this file after CommonLibF4), so `xmake build -r` builds
-- both. Not part of 4Stim; installs to its own mod folder.
target("PapyrusOverdrive")
    set_version("0.1.0")
    add_rules("commonlibf4.plugin", {
        name = "PapyrusOverdrive",
        author = "KeryoWolfe",
        description = "Lets read-only engine functions answer Papyrus scripts at once, and profiles native calls"
    })

    add_defines("PAPYRUSOVERDRIVE_VERSION_MAJOR=0", "PAPYRUSOVERDRIVE_VERSION_MINOR=1", "PAPYRUSOVERDRIVE_VERSION_PATCH=0")  -- keep in step with set_version

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_installfiles("Data/(F4SE/Plugins/*.ini)")
