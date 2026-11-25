libdwcnd_includes = include_directories('include')

libdwcnd = library('dwcnd',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: [nitrodwc_includes, libdwcnd_includes],
    pic: false,
    sources: files(
        'src/DWCi_Nd_attr.c',
        'src/DWCi_Nd_base64.c',
        'src/DWCi_Nd_bgnend.c',
        'src/DWCi_Nd_ca.c',
        'src/DWCi_Nd_cancel.c',
        'src/DWCi_Nd_DWC_NITRO.c',
        'src/DWCi_Nd_fileapi.c',
        'src/DWCi_Nd_filecount.c',
        'src/DWCi_Nd_fileget.c',
        'src/DWCi_Nd_filelist.c',
        'src/DWCi_Nd_os_NITRO.c',
        'src/DWCi_Nd_stdlib_NITRO.c',
        'src/DWCi_Nd_string.c'
    )
)

nitrodwc_libs += libdwcnd
