wayland_scanner = find_program('wayland-scanner')
wayland_protocols_dep = dependency('wayland-protocols')
wayland_protocols_dir = wayland_protocols_dep.get_variable(pkgconfig : 'pkgdatadir')

xdg_shell_xml = wayland_protocols_dir / 'stable' / 'xdg-shell' / 'xdg-shell.xml'

xdg_shell_header = custom_target('xdg-shell-client-header',
  input   : xdg_shell_xml,
  output  : 'xdg-shell-client-protocol.h',
  command : [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
xdg_shell_code = custom_target('xdg-shell-client-code',
  input   : xdg_shell_xml,
  output  : 'xdg-shell-protocol.c',
  command : [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

executable('muzza',
  ['main.c', 'decoder.c', 'ui.c', xdg_shell_header, xdg_shell_code],
  dependencies : [
    flux_dep,
    wayland_client_dep,
    libavformat_dep,
    libavcodec_dep,
    libavutil_dep,
    libswscale_dep,
    libswresample_dep,
    sdl3_dep
  ],
  install      : true,
)
