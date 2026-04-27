project('muzza', 'c',
  version : '0.1.8',
  license : 'Apache-2.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
  ],
)

cc = meson.get_compiler('c')

# Dependencies
flux_proj = subproject('flux')
flux_dep = flux_proj.get_variable('flux_dep')

wayland_client_dep = dependency('wayland-client')
wayland_protocols_dep = dependency('wayland-protocols')

libavformat_dep = dependency('libavformat')
libavcodec_dep = dependency('libavcodec')
libavutil_dep = dependency('libavutil')
libswscale_dep = dependency('libswscale')
libswresample_dep = dependency('libswresample')
sdl3_dep = dependency('sdl3')

# We'll need some Wayland glue for now, similar to flux examples
subdir('src')
