add_rules("mode.release", "mode.debug")
set_policy("build.ccache", true)

add_requires("glfw")
set_languages("cxx20", "c17")

-- Lightweight game core library (no heavy dependencies)
target("GameCore")
do
	set_kind("shared")
	add_files("./src/game_core.cpp")

	if is_plat("macosx") then
		set_arch("arm64")
		set_toolchains("clang")
	end
end

-- Heavy renderer library (all Rive/framework dependencies)
target("PastaRender")
do
	set_kind("shared")
	add_files("./src/rive-render/*.cpp")
	add_files("./src/rive-render/*.mm")
	add_files("./src/renderer_impl.cpp")

	add_packages("glfw")

	--Add rive libraries
	add_linkdirs("./deps/rive-runtime/renderer/out/release/")
	add_links(
		"rive",
		"rive_pls_renderer",
		"rive_decoders",
		"libjpeg",
		"rive_harfbuzz",
		"rive_sheenbidi",
		"rive_yoga",
		"libwebp",
		"libpng",
		"zlib"
	)

	--handle includes
	add_includedirs("./deps/rive-runtime/renderer/glad/")
	add_includedirs("./deps/rive-runtime/renderer/glad/include/")
	add_includedirs("./deps/rive-runtime/renderer/include/")
	add_includedirs("./deps/rive-runtime/include/")
	add_includedirs("./src/rive-render/")
	add_includedirs("./src/")

	if is_plat("macosx") then
		set_arch("arm64")
		set_toolchains("clang")
		-- Link heavy frameworks and rive-only libs here so the app stays thin
		add_frameworks("Metal", "QuartzCore", "IOKIT", "Foundation", "CoreFoundation", "Cocoa", "IOKit", "OpenGL")
	end
end

-- Main executable that depends on both libraries
target("LeftoverPasta")
do
	set_kind("binary")
	set_filename("Leftover Pasta")
	add_files("./src/test.cpp")
	add_files("./src/app_main_new.cpp")

	add_deps("GameCore", "PastaRender") -- Link against both libraries

	--handle includes (app is thin: only local headers)
	add_includedirs("./src/")

	if is_plat("macosx") then
		set_arch("arm64")
		set_toolchains("clang")
		-- Thin app: do not pull heavy frameworks; rely on libraries
		add_rpathdirs("@executable_path")
	end
end
