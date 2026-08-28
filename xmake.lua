add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
-- DearOreUI's header-only public API package. Resolved from the self-hosted
-- xmake-repo (dearoreui-repo) which git-references Dear-OreUI directly, so the
-- build obtains the public headers declaratively via add_requires/add_packages
-- (no manual include path). Local dev may point at the repo checkout instead:
--   add_repositories("dearoreui-repo ../dearoreui-repo")
add_repositories("dearoreui-repo https://github.com/copper-lamp/dearoreui-repo.git")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- add_requires("levilamina x.x.x") for a specific version
-- add_requires("levilamina develop") to use develop version
-- please note that you should add bdslibrary yourself if using dev version
add_requires("levilamina 26.10.*", {configs = {target_type = get_config("target_type")}})

-- DearOreUI public headers (header-only; runtime is resolved via the C ABI
-- bridge from the loaded DearOreUI.dll, so no import library is linked).
add_requires("dearoreui 0.1.1")

add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("my-mod") -- Change this to your mod name.
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    add_packages("levilamina")
    add_packages("dearoreui")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    if is_config("target_type", "server") then
    --  add_includedirs("src-server")
    --  add_files("src-server/**.cpp")
    else
    --  add_includedirs("src-client")
    --  add_files("src-client/**.cpp")
    end

    -- T1: ship the page-script assets (scripts/) next to the mod dll so the
    -- runtime can load them via NativeMod::getModDir() at registration time.
    -- Runs after the modpacker rule (rules' after_build fire before the
    -- target's), so bin/<modName>/ already exists.
    after_build(function(target)
        local mod_define = target:extraconf("rules", "@levibuildscript/modpacker") or {}
        local modName    = mod_define.modName or target:name()
        local srcDir     = path.join(os.scriptdir(), "assets", "scripts")
        if os.isdir(srcDir) then
            local outDir = path.join(os.projectdir(), "bin", modName, "scripts")
            os.mkdir(outDir)
            os.cp(path.join(srcDir, "*"), outDir)
        end
    end)
