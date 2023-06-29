project('fusen', 'cpp',
        license: ['GPLv3'],
        default_options: ['cpp_std=c++17']
)

dependencies = []
dependencies += dependency('Qt6Core')
dependencies += dependency('Qt6Gui')
dependencies += dependency('Qt6Widgets')
dependencies += dependency('sqlite3')
dependencies += dependency('yaml-cpp')

sources = files('fusen/main.cpp', 'fusen/scandirs.cpp', 'fusen/sql.cpp', 'fusen/utils.cpp')
inc =  include_directories('include')
executable('fusen', sources, dependencies: dependencies, include_directories: inc, cpp_args: '-fPIC', install: true)
