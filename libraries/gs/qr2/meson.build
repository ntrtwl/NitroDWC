libdwcgsqr2 = library('dwcgsqr2',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'qr2.c',
        'qr2regkeys.c'
    )
)

nitrodwc_libs += libdwcgsqr2
