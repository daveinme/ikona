project('ikona', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11'])

gtkdep = dependency('gtk+-3.0')

# Windows printer libraries
cc = meson.get_compiler('c')
deps = [gtkdep]

# Define common_sources BEFORE the if/else block
common_sources = [
  'ikona.c',
  'importer.c',
  'viewer.c',
  'utils.c',
  'printer.c',
  'icon_manager.c',
  'editor.c',
  'print_log.c',
  'gui_utils.c'
]

if host_machine.system() == 'windows'
  deps += cc.find_library('winspool', required: true)
  deps += cc.find_library('shell32', required: true)
  rc_file = 'ikona.rc'
  res = custom_target('ikona-res',
    input : rc_file,
    output : 'ikona.o',
    command : ['windres', '-i', '@INPUT@', '-o', '@OUTPUT@', '-O', 'coff'])
  executable('ikona',
    common_sources, # Use common_sources
    res,
    dependencies : deps,
    install : true)
else
  executable('ikona',
    common_sources, # Use common_sources
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
python = import('python')
python_inst = python.find_installation('python3', required: false)
if python_inst.found()
  run_command(
    python_inst,
    '-c',
    'import shutil; shutil.copytree("@0@", "@1@", dirs_exist_ok=True)'.format(
      meson.current_source_dir() / 'icons',
      meson.current_build_dir() / 'icons'),
    check: false
  )
endif

# Copy app icon
configure_file(
  input: 'ikona_icon.png',
  output: 'ikona_icon.png',
  copy: true
)
