project(
	'lifegpu',
	'c',
	default_options: {
		'c_std': 'c23',
		'buildtype': 'debug',
		'b_ndebug': 'if-release',
		'warning_level': '3',
	},
)

wayland_scanner = find_program('wayland-scanner')

wayland_scanner_head = generator(
	wayland_scanner,
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
	output: '@BASENAME@.h',
)

wayland_scanner_code = generator(
	wayland_scanner,
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
	output: '@BASENAME@.c',
)

wayland_protocol = dependency('wayland-protocols').get_variable(
	pkgconfig: 'pkgdatadir',
)

wlr_protocol = dependency('wlr-protocols').get_variable(
	pkgconfig: 'pkgdatadir',
)

protocols = [
	wayland_protocol / 'stable/xdg-shell/xdg-shell.xml',
	wlr_protocol / 'unstable/wlr-layer-shell-unstable-v1.xml',
]

glslc = generator(
	find_program('glslcx'),
	arguments: ['@INPUT@', '-o', '@OUTPUT@'],
	output: '@BASENAME@.spv',
)

sources = run_command('find', 'src', '-name', '*.c', check: true).stdout().split()
shaders = run_command(
	'find',
	'src',
	'-name',
	'*.glsl',
	check: true,
).stdout().split()

name = 'lifegpu'
executable(
	name,
	sources: [
		sources,
		glslc.process(shaders),
		wayland_scanner_head.process(protocols),
		wayland_scanner_code.process(protocols),
	],
	include_directories: [
		include_directories('src'),
	],
	c_args: [
		'-DVERSION="@0@"'.format(meson.project_version()),
		'-Wno-empty-translation-unit',
		'-Wno-pragma-once-outside-header',
		'--embed-dir=@0@/@1@.p'.format(meson.current_build_dir(), name),
	],
	link_args: [],
	dependencies: [
		dependency('sdl3'),
		dependency('wayland-client'),
	],
	install: true,
)
