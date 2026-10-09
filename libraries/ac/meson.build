ac_includes = include_directories('include')

libdwcac = library('dwcac',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep, nitrocrypto_dep],
    include_directories: [nitrodwc_includes, ac_includes],
    pic: false,
    sources: files(
        'src/dwc_ac.c',
        'src/beacon.c',
        'src/callback.c',
        'src/connect.c',
        'src/close.c',
        'src/error.c',
        'src/dwc_ac.c',
        'src/makelist.c',
        'src/retry.c',
        'src/search.c',
        'src/start.c',
        'src/test.c',
        'src/shop_usb.c',
        'src/WDS.c',
        'src/nintendospot.c'
    )
)

nitrodwc_libs += libdwcac