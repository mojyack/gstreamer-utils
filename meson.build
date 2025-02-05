project('gstreamer-utils', 'cpp', version : '0.0', default_options : ['warning_level=3', 'cpp_std=c++23'])
add_global_arguments('-Wfatal-errors', language: 'cpp')

gstreamer_dep = dependency('gstreamer-1.0', version : ['>=1.20'])

executable('simple',
        files('examples/simple.cpp', 'src/pipeline-helper.cpp'),
        dependencies: [gstreamer_dep]
)

executable('appsrcsink',
        files('examples/appsrcsink.cpp', 'src/pipeline-helper.cpp'),
        dependencies: [gstreamer_dep, dependency('gstreamer-app-1.0')]
)

executable('dynamic-pipeline-switch',
        files('examples/dynamic-pipeline-switch.cpp', 'src/pipeline-helper.cpp'),
        dependencies: [gstreamer_dep]
) 

executable('seek',
        files('examples/seek.cpp', 'src/pipeline-helper.cpp'),
        dependencies: [gstreamer_dep]
) 
