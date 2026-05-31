project('noise-reactor', 'cpp',
  version : '0.1.0',
  default_options : ['cpp_std=c++20', 'warning_level=2'])

qt6         = import('qt6')
qt6_dep     = dependency('qt6', modules : ['Core', 'Widgets', 'Gui', 'Concurrent', 'Multimedia'])
sndfile_dep = dependency('sndfile')
fftw3f_dep  = dependency('fftw3f')

inc = include_directories('.')

# RHI headers live in a versioned subdirectory not exported by the default Qt6Gui
# include path.  Derive it from pkg-config so it stays correct across Qt upgrades.
_qt_inc = run_command('pkg-config', '--variable=includedir', 'Qt6Core',
                      check : true).stdout().strip()
_qt_ver = qt6_dep.version()
rhi_inc = include_directories(_qt_inc / 'QtGui' / _qt_ver / 'QtGui')

# ── GLSL → QSB (Qt shader bundle) ────────────────────────────────────────────
# qsb lives in Qt's private bin dir on Linux; on other platforms it's in PATH.
qsb = find_program(['/usr/lib/qt6/bin/qsb', 'qsb6', 'qsb'], required : true)
rcc = find_program(['/usr/lib/qt6/rcc', '/usr/lib/qt6/bin/rcc', 'rcc6', 'rcc'], required : true)

# --spirv embeds SPIR-V alongside GLSL so Qt's D3D11/Metal backends can
# cross-compile to HLSL/MSL at runtime via the built-in SPIRV-Cross.
vert_qsb = custom_target('image_vert_qsb',
  input   : 'shaders/image.vert',
  output  : 'image.vert.qsb',
  command : [qsb, '--glsl', '300 es,330', '--spirv', '-o', '@OUTPUT@', '@INPUT@'],
)

frag_qsb = custom_target('image_frag_qsb',
  input   : 'shaders/image.frag',
  output  : 'image.frag.qsb',
  command : [qsb, '--glsl', '300 es,330', '--spirv', '-o', '@OUTPUT@', '@INPUT@'],
)

composite_qsb = custom_target('composite_frag_qsb',
  input   : 'shaders/composite.frag',
  output  : 'composite.frag.qsb',
  command : [qsb, '--glsl', '300 es,330', '--spirv', '-o', '@OUTPUT@', '@INPUT@'],
)

# Copy the QRC into the build directory so rcc resolves the .qsb references
# relative to the same directory where the generated shader bundles land.
shader_qrc = configure_file(
  input  : 'shaders/shaders.qrc',
  output : 'shaders.qrc',
  copy   : true,
)

shader_res = custom_target('shader_resources',
  input   : shader_qrc,
  output  : 'qrc_shaders.cpp',
  command : [rcc, '@INPUT@', '--name', 'shaders', '-o', '@OUTPUT@'],
  depends : [vert_qsb, frag_qsb, composite_qsb],
)

# ── C++ sources ───────────────────────────────────────────────────────────────
sources = files(
  'noise_reactor/main.cc',
  'noise_reactor/app.cc',
  'noise_reactor/audio_file.cc',
  'noise_reactor/audio_analyzer.cc',
  'noise_reactor/preview_widget.cc',
  'noise_reactor/waveform_widget.cc',
  'noise_reactor/cinematic_target_widget.cc',
)

moc_files = qt6.preprocess(
  moc_headers     : ['noise_reactor/app.h', 'noise_reactor/preview_widget.h',
                     'noise_reactor/waveform_widget.h',
                     'noise_reactor/cinematic_target_widget.h'],
  include_directories : inc,
  dependencies    : qt6_dep,
)

executable('noise_reactor',
  sources,
  moc_files,
  shader_res,
  dependencies    : [qt6_dep, sndfile_dep, fftw3f_dep],
  include_directories : [inc, rhi_inc],
  install         : true,
)
