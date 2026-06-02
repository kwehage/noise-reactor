project('kaos-reactor', 'cpp',
  version : '0.0.2',
  default_options : ['cpp_std=c++20', 'warning_level=2'])

qt6         = import('qt6')
fs          = import('fs')
qt6_dep     = dependency('qt6', modules : ['Core', 'Widgets', 'Gui', 'Concurrent', 'Multimedia'],
                         version : '>=6.6')
sndfile_dep = dependency('sndfile')
fftw3f_dep  = dependency('fftw3f')

inc = include_directories('.')

# RHI headers live in a versioned subdirectory not exported by the default Qt6Gui
# include path.  Derive it from pkg-config (distro Qt) or qmake (aqtinstall).
_qt_inc = run_command('pkg-config', '--variable=includedir', 'Qt6Core', check : false).stdout().strip()
if _qt_inc == ''
  _r = run_command('qmake6', '-query', 'QT_INSTALL_HEADERS', check : false)
  if _r.returncode() != 0
    _r = run_command('qmake', '-query', 'QT_INSTALL_HEADERS', check : true)
  endif
  _qt_inc = _r.stdout().strip()
endif
_qt_ver = qt6_dep.version()
rhi_inc = include_directories(_qt_inc / 'QtGui' / _qt_ver / 'QtGui')

# ── GLSL → QSB (Qt shader bundle) ────────────────────────────────────────────
# Qt's host-tool dir (rcc, qsb, etc.) is not always in PATH.
# Build a list of candidate directories from all known pkg-config variables.
_qt_tool_dirs = []
foreach _var : ['host_bins', 'bindir', 'libexecdir', 'host_libexecs']
  _d = run_command('pkg-config', '--variable=' + _var, 'Qt6Core',
                   check : false).stdout().strip()
  if _d != ''
    _qt_tool_dirs += [_d]
  endif
endforeach

_qsb_candidates = []
_rcc_candidates = []
foreach _dir : _qt_tool_dirs
  _qsb_candidates += [_dir / 'qsb', _dir / 'qsb6']
  _rcc_candidates += [_dir / 'rcc', _dir / 'rcc6']
endforeach
_qsb_candidates += ['qsb6', 'qsb']
_rcc_candidates += ['rcc6', 'rcc']

qsb = find_program(_qsb_candidates, required : true)
rcc = find_program(_rcc_candidates, required : true)

vert_qsb = custom_target('image_vert_qsb',
  input   : 'shaders/image.vert',
  output  : 'image.vert.qsb',
  command : [qsb, '--glsl', '300 es,330', '-o', '@OUTPUT@', '@INPUT@'],
)

frag_qsb = custom_target('image_frag_qsb',
  input   : 'shaders/image.frag',
  output  : 'image.frag.qsb',
  command : [qsb, '--glsl', '300 es,330', '-o', '@OUTPUT@', '@INPUT@'],
)

composite_qsb = custom_target('composite_frag_qsb',
  input   : 'shaders/composite.frag',
  output  : 'composite.frag.qsb',
  command : [qsb, '--glsl', '300 es,330', '-o', '@OUTPUT@', '@INPUT@'],
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
  'kaos/reactor/main.cc',
  'kaos/reactor/app.cc',
  'kaos/reactor/audio_file.cc',
  'kaos/reactor/audio_analyzer.cc',
  'kaos/reactor/preview_widget.cc',
  'kaos/reactor/waveform_widget.cc',
  'kaos/reactor/cinematic_target_widget.cc',
)

moc_files = qt6.preprocess(
  moc_headers     : ['kaos/reactor/app.h', 'kaos/reactor/preview_widget.h',
                     'kaos/reactor/waveform_widget.h',
                     'kaos/reactor/cinematic_target_widget.h'],
  include_directories : inc,
  dependencies    : qt6_dep,
)

executable('kaos-reactor',
  sources,
  moc_files,
  shader_res,
  dependencies    : [qt6_dep, sndfile_dep, fftw3f_dep],
  include_directories : [inc, rhi_inc],
  install         : true,
)
