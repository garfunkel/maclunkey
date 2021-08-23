project('maclunkey', 'c',
	version: '0.1',
	default_options: ['c_std=c11', 'warning_level=3'])

#imageMagick = dependency('MagickWand', version: '>=7.0.0')

executable('server',
	['server.c', 'utils.c'],
	#dependencies: imageMagick,
	install: true)

executable('client',
	['client.c', 'utils.c'],
	#dependencies: imageMagick,
	install: true)
