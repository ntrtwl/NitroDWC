libdwcgsgstats = library('dwcgsgstats',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'gbucket.c',
        'gstats.c'
    )
)

nitrodwc_libs += libdwcgsgstats
