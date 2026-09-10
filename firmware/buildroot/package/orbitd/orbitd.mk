################################################################################
#
# orbitd
#
################################################################################

ORBITD_VERSION = 1.0.0
ORBITD_SITE = $(BR2_EXTERNAL_EARBRAIN_PATH)/../orbitd
ORBITD_SITE_METHOD = local
ORBITD_LICENSE = Proprietary
ORBITD_DEPENDENCIES = alsa-lib bluez5_utils sqlcipher

ORBITD_CONF_OPTS = -DCROSS_COMPILE=ON -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF

$(eval $(cmake-package))
