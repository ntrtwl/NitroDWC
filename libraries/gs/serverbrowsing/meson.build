libdwcgsserverbrowsing = library('dwcgsserverbrowsing',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'sb_crypt.c',
        'sb_queryengine.c',
        'sb_server.c',
        'sb_serverbrowsing.c',
        'sb_serverlist.c'
    )
)

nitrodwc_libs += libdwcgsserverbrowsing
