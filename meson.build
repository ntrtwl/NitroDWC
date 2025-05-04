project('NitroDWC', 'c',
    default_options: {
        'default_library': 'static',
        'prefer_static':    true
    },
    meson_version: '>=1.2.0',
    version: '2.2.30008'
)

fs = import('fs')

buildtype = get_option('buildtype')
cc = meson.get_compiler('c')

############################################################
###                    COMPILER FLAGS                    ###
############################################################

if not meson.is_subproject() and buildtype != 'plain'
c_args = [
    '-O4,p',
    '-proc', 'arm946e',
    '-enum', 'int',
    '-lang', 'c99',
    '-Cpp_exceptions', 'off',
    '-gccext,on',
    '-msgstyle', 'gcc',
    '-ipa', 'file',
    '-interworking',
    '-inline', 'on,noauto',
    '-char', 'signed',
    '-nosyspath',
    '-stdinc'
]
else
c_args = []
endif

mwrap_ver = get_option('mwrap_ver')
if mwrap_ver != 'default'
    c_args += ['-wrap:ver', mwrap_ver]
endif

mwrap_sdk = get_option('mwrap_sdk')
if mwrap_sdk != 'default'
    c_args += ['-wrap:sdk', mwrap_sdk]
endif

add_project_arguments(c_args,
    language: 'c',
    native: false
)

nitrodwc_args = [
    '-D_NITRO',
    '-DSDK_CW_FORCE_EXPORT_SUPPORT',
    '-DSDK_TS',
    '-DSDK_4M',
    '-DSDK_ARM9',
    '-DSDK_CW',
    '-DSDK_FINALROM',
    '-DSDK_CODE_ARM',
    '-DNNS_FINALROM'
]

if get_option('link_ppwlobby')
    nitrodwc_args += '-DLINK_PPWLOBBY'
endif

add_project_arguments(nitrodwc_args,
    language: 'c',
    native: false
)

############################################################
###                    INCLUDE PATHS                     ###
############################################################
nitrodwc_includes = include_directories(
    'include',
    'include/gs',
    'include/base'
)

############################################################
###                     DEPENDENCIES                     ###
############################################################
nitrosdk_dep    = dependency('NitroSDK')
nitrosystem_dep = dependency('NitroSystem')
nitrowifi_dep   = dependency('NitroWiFi')

############################################################
###                      LIBRARIES                       ###
############################################################
subdir('libraries')

############################################################
###                    FOR CONSUMERS                     ###
############################################################
nitrodwc_dep = declare_dependency(
    compile_args: nitrodwc_args,
    include_directories: nitrodwc_includes,
    link_with: nitrodwc_libs + nitrodwc_prebuilt
)

meson.override_dependency('NitroDWC', nitrodwc_dep)
