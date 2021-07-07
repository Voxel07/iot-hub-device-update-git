#Simple FS-Skript to generate all relevant update files

#!/bin/bash

#abbort skript if something failes
set -e

echo $(date)
deploydir=/home/developer/gitLocal/yocto-fus/build-fsimx8mm-fus-imx-wayland/tmp/deploy/images/fsimx8mm
tausch=/mnt/fs-temp/Schneider/tausch
tftpboot=/tftpboot
echo "Kopiere squashfs"
cp $deploydir/fus-image-update-std-fsimx8mm.nand.squashfs /tftpboot/r.fs
echo "Kopiere ubifs"
cp $deploydir/fus-image-update-std-fsimx8mm.data-partition-nand.ubifs /tftpboot/d.fs
echo "Kopiere firmware update"
cp $deploydir/rauc_update_nand.artifact $tausch
echo "Kopiere application update"
cp $tftpboot/appUpdate $tausch
echo "Erstelle frimware manifest"
/home/developer/gitLocal/iot-hub-device-update-git/tools/AduCmdlets/create-adu-import-manifest.sh -p 'FuS' -n 'Frimware' -v '1.0' -t 'fus/fsupdate:firmware' -i '2.0' -c aduc_FUS,aduc_IMX8MM $tausch/rauc_update_nand.artifact > $tausch/frimwareManifest.json
echo "Erstelle application manifest"
/home/developer/gitLocal/iot-hub-device-update-git/tools/AduCmdlets/create-adu-import-manifest.sh -p 'FuS' -n 'Application' -v '1.0' -t 'fus/fsupdate:application' -i '2.0' -c aduc_FUS,aduc_IMX8MM $tausch/appUpdate > $tausch/applicationManifest.json
