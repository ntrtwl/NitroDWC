libdwcgschat = library('dwcgschat',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'chatCallbacks.c',
        'chatChannel.c',
        'chatCrypt.c',
        'chatHandlers.c',
        'chatMain.c',
        'chatSocket.c'
    )
)

nitrodwc_libs += libdwcgschat
