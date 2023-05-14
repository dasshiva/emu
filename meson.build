project('emu86', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])
src = ['src/main.c', 'src/loader.c']
exe = executable('emu86', src, include_directories: 'include',
  install : true)

test('basic', exe)
