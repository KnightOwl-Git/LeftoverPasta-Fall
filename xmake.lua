add_rules("mode.release", "mode.debug")

add_requires("glfw")
set_languages("cxx20", "c17")

target("LeftoverPasta")
do
	set_kind("binary")
	set_filename("Leftover Pasta")
	add_files("./src/*.cpp")

	add_files("./src/rive-render/*.cpp")
	add_files("./src/rive-render/*.mm")
	add_rules("xcode.application")

	-- add rive files to build- is there a better way to do this?

	set_configdir("$(builddir)/$(plat)/$(arch)/$(mode)/riv")
	add_configfiles("./src/riv/lp_level_editor.riv")

	--Add rive libraries
	add_linkdirs("./deps/rive-runtime/renderer/out/release/")
	add_links(
		"rive",
		"rive_pls_renderer",
		"rive_decoders",
		"rive_harfbuzz",
		"rive_sheenbidi",
		"rive_yoga",
		"libwebp",
		"libpng",
		"zlib"
	)

	add_packages("glfw")
	--handle includes
	add_includedirs("./deps/rive-runtime/renderer/glad/")
	add_includedirs("./deps/rive-runtime/renderer/glad/include/")
	add_includedirs("./deps/rive-runtime/renderer/include/")
	add_includedirs("./deps/rive-runtime/include/")
	add_includedirs("./src/rive-render/")
	add_includedirs("./src/")
	add_cxflags("-Isrc/asset_utils.hpp")

	if is_plat("macosx") then
		set_arch("arm64")
		set_toolchains("clang")
		add_frameworks("Cocoa", "Metal", "QuartzCore", "IOKIT", "Foundation")
	end
end
