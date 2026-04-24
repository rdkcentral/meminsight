SUMMARY = "Memory insight utility and runner service"
DESCRIPTION = "Collects meminfo, per-process memory stats, and optional JSON/fragmentation reports"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=1c020dfe1abb4e684874a44de1244c28"

SRC_URI = "file://meminsight"
S = "${WORKDIR}/meminsight"

inherit autotools systemd

PACKAGECONFIG ??= ""
PACKAGECONFIG[cjson] = "--enable-cjson,,"

SYSTEMD_SERVICE:${PN} = "meminsight.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

EXTRA_OECONF += "${@bb.utils.contains('PACKAGECONFIG', 'cjson', '--enable-cjson', '', d)}"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/deploy/systemd/meminsight.service ${D}${systemd_system_unitdir}/meminsight.service
}
