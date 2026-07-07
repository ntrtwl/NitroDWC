libdwcbm = library('dwcbm',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        'src/dwc_backup.c',
        'src/dwc_bm_init.c',
        'src/util_wifiidtool.c'
    )
)

nitrodwc_libs += libdwcbm
