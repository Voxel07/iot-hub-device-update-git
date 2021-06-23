#Simple FS-Skript to generate all relevant update files

#!/bin/bash
echo $(date)
deploydir=/home/developer/gitLocal/yocto-fus/build-fsimx8mm-fus-imx-wayland/tmp/deploy/images/fsimx8mm
tausch=/mnt/fs-temp/Schneider/tausch
echo "Kopiere squashfs"
cp $deploydir/fus-image-update-std-fsimx8mm.nand.squashfs /tftpboot/r.fs
echo "Kopiere update"
cp $deploydir/rauc_update_nand.artifact $tausch
echo "Erstelle manifest"
/home/developer/gitLocal/iot-hub-device-update-git/tools/AduCmdlets/create-adu-import-manifest.sh -p 'FuS' -n 'ApplyTest' -v '2.0' -t 'fus/fsupdate:firmware' -i '2.0' -c aduc_FUS,aduc_IMX8MM $tausch/rauc_update_nand.artifact > $tausch/manifest.json
