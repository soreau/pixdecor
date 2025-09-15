project(
    'pixdecor',
    'c',
    'cpp',
    version: '0.1.0',
    license: 'MIT',
    meson_version: '>=0.59.0',
    default_options: [
        'cpp_std=c++17',
        'c_std=c11',
    ],
)

add_project_arguments(['-DWLR_USE_UNSTABLE'], language: ['cpp', 'c'])
add_project_arguments(['-DWAYFIRE_PLUGIN'], language: ['cpp', 'c'])
add_project_link_arguments(['-rdynamic','-fPIC'], language:'cpp')

wayfire = dependency('wayfire', version: '>=0.9.0')


subdir('src')
subdir('metadata')

share_dir = join_paths(get_option('prefix'), 'share')
wayfire_locale_dir = join_paths(share_dir, 'locale')

fs = import('fs')

mo_targets = []

po_files_raw = run_command('find', 'locale', '-name', '*.po').stdout().strip().split('\n')
po_files = files(po_files_raw)

foreach po_file : po_files
  language = fs.name(fs.parent(fs.parent(po_file)))
  mo_file = fs.stem(po_file) + '.mo'
  t = custom_target('mo-' + language,
    input : po_file,
    output : mo_file,
    command : ['msgfmt', '@INPUT@', '-o', '@OUTPUT@'],
    install : true,
    install_dir : join_paths(
      wayfire_locale_dir,
      language,
      'LC_MESSAGES'
    )
  )

  mo_targets += t
endforeach
