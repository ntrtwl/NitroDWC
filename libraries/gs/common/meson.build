libdwcgscommon = library('dwcgscommon',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: nitrodwc_includes,
    pic: false,
    sources: files(
        '../available.c',
        '../darray.c',
        '../hashtable.c',
        '../md5c.c',
        '../nonport.c'
    )
)

nitrodwc_libs += libdwcgscommon
