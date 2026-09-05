project('nabu-sar-service', 'c', version: '0.2.3',
  default_options: ['c_std=gnu11', 'warning_level=3'])

glib = dependency('glib-2.0')
gio = dependency('gio-2.0')
gio_unix = dependency('gio-unix-2.0')
libssc = dependency('libssc')
m = meson.get_compiler('c').find_library('m')
inc = include_directories('src')
core = static_library('nabu-sar-core', 'src/sar-parser.c', 'src/sar-state.c',
  include_directories: inc, dependencies: [glib, m])

executable('nabu-sar-service', 'src/nabu-sar-service.c',
  include_directories: inc, link_with: core,
  dependencies: [glib, gio, gio_unix, libssc, m],
  install: true, install_dir: get_option('libexecdir'))

executable('nabu-sar-control', 'src/nabu-sar-control.c',
  dependencies: [glib, gio], install: true,
  install_dir: get_option('libexecdir'))

executable('nabu-cct-iio-bridge', 'src/nabu-cct-iio-bridge.c',
  dependencies: [glib, gio, gio_unix, libssc, m], install: true,
  install_dir: get_option('libexecdir'))

install_data('data/nabu-sar.conf', install_dir: get_option('sysconfdir'))
install_data('data/nabu-sar-service.service',
  install_dir: get_option('prefix') / 'lib/systemd/system')
install_data('data/org.senemos.Nabu.Sar.conf',
  install_dir: get_option('datadir') / 'dbus-1/system.d')
install_data('data/nabu-cct-iio-bridge.service',
  install_dir: get_option('prefix') / 'lib/systemd/system')
install_data('data/nabu-cct-iio.conf',
  install_dir: get_option('prefix') / 'lib/modules-load.d')
install_data('tools/nabu-cct-iio-setup', install_mode: 'rwxr-xr-x',
  install_dir: get_option('libexecdir'))
install_data('tools/nabu-sar-capture', install_mode: 'rwxr-xr-x',
  install_dir: get_option('bindir'))

test('sar-core', executable('test-sar', 'tests/test-sar.c',
  include_directories: inc, link_with: core, dependencies: [glib, m]))
