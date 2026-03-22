libdwcgsgp = library('dwcgsgp',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'gp.c',
        'gpi.c',
        'gpiBuddy.c',
        'gpiBuffer.c',
        'gpiCallback.c',
        'gpiConnect.c',
        'gpiInfo.c',
        'gpiOperation.c',
        'gpiPeer.c',
        'gpiProfile.c',
        'gpiSearch.c',
        'gpiTransfer.c',
        'gpiUnique.c',
        'gpiUtility.c'
    )
)

nitrodwc_libs += libdwcgsgp

