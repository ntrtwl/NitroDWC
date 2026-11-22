libdwcnhttp_includes = include_directories('include')

libdwcnhttp = library('dwcnhttp',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: [nitrodwc_includes, libdwcnhttp_includes],
    pic: false,
    sources: files(
        'src/NHTTP_bgnend.c',
        'src/NHTTP_control.c',
        'src/NHTTP_list.c',
        'src/NHTTP_os_NITRO.c',
        'src/NHTTP_recvbuf.c',
        'src/NHTTP_request.c',
        'src/NHTTP_resolve_NITRO.c',
        'src/NHTTP_response.c',
        'src/NHTTP_socket_NITRO.c',
        'src/NHTTP_stdlib_NITRO.c',
        'src/NHTTP_thread.c',
        'src/NHTTP_token.c'
    )
)

nitrodwc_libs += libdwcnhttp
