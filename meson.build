project('maclunkey', 'c',
	version: '0.1',
	default_options: ['c_std=c11', 'warning_level=3'])

if get_option('buildtype').startswith('debug')
	add_project_arguments('-DDEBUG', language: 'c')
endif

add_project_arguments('-D_GNU_SOURCE', language: 'c')

dependencies = dependency('threads')
#imageMagick = dependency('MagickWand', version: '>=7.0.0')

executable('server',
	['server.c', 'packets.c', 'utils.c'],
	dependencies: dependencies,
	install: true)

executable('client',
	['client.c', 'packets.c', 'utils.c'],
	dependencies: dependencies,
	install: true)
