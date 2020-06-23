DESCRIPTION = "TSN use space gptp utilities"
SECTION = "gptp"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI ="\
    file://Open-AVB \
"

FILESEXTRAPATHS_prepend := "${TSNPACK}/others/:"

S = "${WORKDIR}"

TARGET_CC_ARCH += "${LDFLAGS}"

do_compile () {
    mkdir -p ${TSNPACK}/others/Open-AVB/daemons/gptp/linux/build/obj
    make -C ${TSNPACK}/others/Open-AVB/daemons/gptp/linux/build clean
    #make -C ${TSNPACK}/others/Open-AVB tsn_talker_clean
    #make -C ${TSNPACK}/others/Open-AVB tsn_listener_clean
    make -C ${TSNPACK}/others/Open-AVB udp_client_clean
    make -C ${TSNPACK}/others/Open-AVB udp_server_clean
    make -C ${TSNPACK}/others/Open-AVB/daemons/gptp/linux/build
    #make -C ${TSNPACK}/others/Open-AVB tsn_talker
    #make -C ${TSNPACK}/others/Open-AVB tsn_listener
    make -C ${TSNPACK}/others/Open-AVB udp_client
    make -C ${TSNPACK}/others/Open-AVB udp_server
}

do_install () {
	export DIST_ROOT=${D}
 	install -d ${D}${sbindir}
	install -m 755 ${TSNPACK}/others/Open-AVB/daemons/gptp/linux/build/obj/daemon_cl ${D}${sbindir}/
	install -m 755 ${TSNPACK}/others/Open-AVB/examples/tsn_talker/tsn_talker ${D}${sbindir}/
	install -m 755 ${TSNPACK}/others/Open-AVB/examples/tsn_listener/tsn_listener ${D}${sbindir}/
}
