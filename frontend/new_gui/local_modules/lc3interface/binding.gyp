{
    'targets': [
        {
            'include_dirs': [
                "<!(node -e \"require('nan')\")",
                '<(module_root_dir)/../../../../backend/src'
            ],
            'target_name': 'lc3interface',
            'sources': ['wrapper.cpp'],
            'cflags!': ['-fno-exceptions'],
            'cflags_cc!': ['-fno-exceptions'],
            'conditions': [['OS=="mac"', {
                'xcode_settings': {
                    'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
                }
            }]],
            'link_settings': {
                'libraries': ['-L<(module_root_dir)/../../../../build/lib', '-llc3core', "-Wl,-rpath,'<(module_root_dir)/../../../../build/lib'"]
            }
        }
    ]
}
