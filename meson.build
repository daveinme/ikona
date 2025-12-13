project('ikona', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11'])

gtkdep = dependency('gtk+-3.0')

executable('ikona',
  'ikona.c',
  'importer.c',
  'viewer.c',
  'utils.c',
  'printer.c',
  dependencies : gtkdep,
  install : true)
