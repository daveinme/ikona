project('ikona', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11'])

gtkdep = dependency('gtk+-3.0')

# Windows printer libraries
cc = meson.get_compiler('c')
deps = [gtkdep]

if host_machine.system() == 'windows'
  deps += cc.find_library('winspool', required: true)
  deps += cc.find_library('shell32', required: true)
  rc_file = 'ikona.rc'
  res = custom_target('ikona-res',
    input : rc_file,
    output : 'ikona.o',
    command : ['windres', '-i', '@INPUT@', '-o', '@OUTPUT@', '-O', 'coff'])
  executable('ikona',
    'ikona.c',
    'importer.c',
    'viewer.c',
    'utils.c',
    'printer.c',
    'icon_manager.c',
    res,
    dependencies : deps,
    install : true)
else
  executable('ikona',
    'ikona.c',
    'importer.c',
    'viewer.c',
    'utils.c',
    'printer.c',
    'icon_manager.c',
    dependencies : deps,
    install : true)
endif

# Copy CSS file to build directory
configure_file(
  input: 'style.css',
  output: 'style.css',
  copy: true
)

# Copy icons directory to build directory
run_command('mkdir', '-p', '@0@/icons'.format(meson.current_build_dir()),
  check: false)
run_command('cp', '-r', '@0@/icons/'.format(meson.current_source_dir()),
  '@0@/'.format(meson.current_build_dir()),
  check: false)

# Copy app icon
configure_file(
  input: 'ikona_icon.png',
  output: 'ikona_icon.png',
  copy: true
)
