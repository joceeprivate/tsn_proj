DESCRIPTION = "TCNopen"
SECTION = "trdp"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI ="\
    file://trdp \
"

FILESEXTRAPATHS_prepend := "${TSNPACK}/others/:"

S = "${WORKDIR}"

TARGET_CC_ARCH += "${LDFLAGS}"

do_compile () {	
    make -C ${TSNPACK}/others/trdp distclean
    make -C ${TSNPACK}/others/trdp LINUX_config
    make -C ${TSNPACK}/others/trdp all
}

do_install () {
    export DIST_ROOT=${D}
    install -d ${D}${sbindir}
    install -m 0755 ${TSNPACK}/others/trdp/bld/output/linux-dbg/sendHello ${D}${sbindir}/
    install -m 0755 ${TSNPACK}/others/trdp/bld/output/linux-dbg/sendData ${D}${sbindir}/
    install -m 0755 ${TSNPACK}/others/trdp/bld/output/linux-dbg/receiveHello ${D}${sbindir}/
}
