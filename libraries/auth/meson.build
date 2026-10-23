libdwcauth_includes = include_directories('include')

libdwcauth = library('dwcauth',
    dependencies: [nitrosdk_dep, nitrosystem_dep, nitrowifi_dep],
    include_directories: [nitrodwc_includes, libdwcauth_includes],
    pic: false,
    sources: files(
        'src/dwc_auth.c',
        'src/dwc_cal.c',
        'src/dwc_http.c',
        'src/dwc_netcheck.c',
        'src/dwc_nastime.c',
        'src/dwc_svl.c',
        'src/dwc_prof.c',
        'src/util_base64.c',
        'src/dwc_ca_cybertrust_globalsign.c',
        'src/dwc_ca_cybertrust_baltimore.c',
        'src/dwc_ca_cybertrust_gteglroot.c',
        'src/dwc_ca_cybertrust_gteroot.c',
        'src/dwc_ca_nas_self.c',
        'src/dwc_ca_thawte_premiumserver.c',
        'src/dwc_ca_thawte_server.c',
        'src/dwc_ca_verisign_c3pcag2v2.c',
        'src/dwc_ca_verisign_c3pcag3v2.c',
        'src/dwc_ca_verisign_pca3ssv4.c',
        'src/dwc_ca_verisign_secureserver.c',
        'src/dwc_ca_globalsign_rootcar2.c'
    )
)

nitrodwc_libs += libdwcauth
