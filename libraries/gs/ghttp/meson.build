libdwcgsghttp = library('dwcgsghttp',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'ghttpBuffer.c',
        'ghttpCallbacks.c',
        'ghttpCommon.c',
        'ghttpConnection.c',
        'ghttpEncryption.c',
        'ghttpMain.c',
        'ghttpPost.c',
        'ghttpProcess.c'
    )
)

nitrodwc_libs += libdwcgsghttp
