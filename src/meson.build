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

core_deps = [
  flux_dep,
  libavformat_dep,
  libavcodec_dep,
  libavutil_dep,
  libswscale_dep,
  libswresample_dep,
  sdl3_dep
]

executable('muzza',
  [
    'main.c',
    'app.c',
    'decoder.c',
    'exporter.c',
    'import_browser.c',
    'path_utils.c',
    'playback_controller.c',
    'project.c',
    'ui.c',
    'ui_export_panel.c',
    'ui_import_browser_panel.c',
    'ui_media_panel.c',
    'ui_monitor_panel.c',
    'ui_shared.c',
    'ui_text.c',
    'ui_timeline_panel.c',
    xdg_shell_header,
    xdg_shell_code
  ],
  dependencies : [
    core_deps,
    wayland_client_dep,
  ],
  install      : true,
)

project_test = executable('muzza-project-test',
  ['test_project.c', 'project.c', 'decoder.c', 'path_utils.c'],
  dependencies : core_deps,
)

core_test = executable('muzza-core-test',
  ['test_muzza_core.c', 'project.c', 'decoder.c', 'path_utils.c'],
  dependencies : core_deps,
)

test('project-roundtrip', project_test)
test('core-regression', core_test)
