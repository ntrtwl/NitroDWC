libdwcnatneg = library('dwcgsnatneg',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'natneg.c'
    )
)

nitrodwc_libs += libdwcnatneg
