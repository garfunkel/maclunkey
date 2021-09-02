project('maclunkey', 'c',
	version: '0.1',
	license: 'MIT',
	default_options: ['c_std=c17', 'warning_level=3'])

description = 'Terminal-based video conferencing application.'

add_project_arguments('-DAPP_NAME="' + meson.project_name() + '"', language: 'c')
add_project_arguments('-DAPP_VERSION="' + meson.project_version() + '"', language: 'c')
add_project_arguments('-DAPP_DESC="' + description + '"', language: 'c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')

if get_option('buildtype').startswith('debug')
	add_project_arguments('-DDEBUG', language: 'c')
endif

dependencies = dependency('threads')
#imageMagick = dependency('MagickWand', version: '>=7.0.0')

executable('server',
	['server.c', 'packets.c', 'utils.c'],
	dependencies: dependencies,
	install: true)

executable('client',
	['client.c', 'packets.c', 'utils.c', 'drawing.c'],
	dependencies: dependencies,
	install: true)
