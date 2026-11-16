# Copies the shared agent built beside the app (library/boreale-link/agent/out/
# boreale-link.nsp) into the romfs, as sysmodules/agent/exefs.nsp with a
# version.txt (app version, commit, SHA-256): what the kit's agent screens
# install from (boreale-kit's util/sysmodule). An agent not built (the desktop
# and web builds, a checkout without devkitPro) is left out: the screens then
# say so.
#   cmake -DSOURCE_DIR=… -DDEST=… -DAPP_VERSION=… -DAPP_COMMIT=… -P bundle_sysmodules.cmake
set(src "${SOURCE_DIR}/library/boreale-link/agent/out/boreale-link.nsp")
if (EXISTS "${src}")
    file(MAKE_DIRECTORY "${DEST}/agent")
    configure_file("${src}" "${DEST}/agent/exefs.nsp" COPYONLY)
    file(SHA256 "${src}" hash)
    file(WRITE "${DEST}/agent/version.txt" "version=${APP_VERSION}\ncommit=${APP_COMMIT}\nsha256=${hash}\n")
    message(STATUS "romfs: sysmodule agent ${hash}")
endif ()
