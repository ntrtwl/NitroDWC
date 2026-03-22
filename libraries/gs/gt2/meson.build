libdwcgsgt2 = library('dwcgsgt2',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'gt2Auth.c',
        'gt2Buffer.c',
        'gt2Callback.c',
        'gt2Connection.c',
        'gt2Encode.c',
        'gt2Filter.c',
        'gt2Main.c',
        'gt2Message.c',
        'gt2Socket.c',
        'gt2Utility.c'
    )
)

nitrodwc_libs += libdwcgsgt2
