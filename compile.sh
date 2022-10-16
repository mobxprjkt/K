#!/bin/bash
#
# Compile script for ? kernel
# Copyright (C) 2020-2021 Adithya R.

SECONDS=0 # builtin bash timer
ZIPNAME="/tmp/output/SunriseTestAdreno-juice_$(date +%Y%m%d-%H%M).zip"
TC_DIR="$HOME/tc/sdclang"
GCC_DIR="$HOME/tc/gcc"
GCC64_DIR="$HOME/tc/gcc64"
DTC_DIR="$HOME/tc/dtc"
AK3_DIR="$HOME/android/AnyKernel3"
DEFCONFIG="vendor/bengal-perf_defconfig"

mkdir -p /tmp/output

env() {
export TELEGRAM_BOT_TOKEN=""
export TELEGRAM_CHAT_ID="@SunriseCI"

TRIGGER_SHA="$(git rev-parse HEAD)"
LATEST_COMMIT="$(git log --pretty=format:'%s' -1)"
COMMIT_BY="$(git log --pretty=format:'by %an' -1)"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"
KERNEL_VERSION="$(cat out/.config | grep Linux/arm64 | cut -d " " -f3)"
export FILE_CAPTION="
🏚️ Linux version: $KERNEL_VERSION
🌿 Branch: $BRANCH
🎁 Top commit: $LATEST_COMMIT
👩‍💻 Commit author: $COMMIT_BY"
}

export PATH="${TC_DIR}/compiler/bin:${GCC64_DIR}/bin:${GCC_DIR}/bin:/usr/bin:${PATH}"
export DTC_EXT="${DTC_DIR}/linux-x86/dtc/dtc"


curl -F document=@"${ZIPNAME}" -F "caption=${FILE_CAPTION}" "https://api.telegram.org/bot${TELEGRAM_BOT_TOKEN}/sendDocument?chat_id=${TELEGRAM_CHAT_ID}&parse_mode=Markdown"
if ! [ -d "$TC_DIR" ]; then
echo "sdclang not found! Cloning to $TC_DIR..."
if ! git clone -q -b 14 --depth=1 https://github.com/ThankYouMario/proprietary_vendor_qcom_sdclang $TC_DIR; then
echo "Cloning failed! Aborting..."
exit 1
fi
fi

if ! [ -d "$GCC_DIR" ]; then
echo "GCC not found! Cloning to $GCC_DIR..."
if ! git clone -q -b master --depth=1 https://github.com/Enprytna/arm-linux-androideabi-4.9 $GCC_DIR; then
echo "Cloning failed! Aborting..."
exit 1
fi
fi

if ! [ -d "$GCC64_DIR" ]; then
echo "GCC64 not found! Cloning to $GCC64_DIR..."
if ! git clone -q -b master --depth=1 https://github.com/Enprytna/aarch64-linux-android-4.9 $GCC64_DIR; then
echo "Cloning failed! Aborting..."
exit 1
fi
fi

if ! [ -d "$DTC_DIR" ]; then
echo "DTC not found! Cloning to $DTC_DIR..."
if ! git clone -q -b android10-gsi --depth=1 https://android.googlesource.com/platform/prebuilts/misc $DTC_DIR; then
echo "Cloning failed! Aborting..."
exit 1
fi
fi

export KBUILD_BUILD_USER=ShelbyHell
export KBUILD_BUILD_HOST=Instance

if [[ $1 = "-r" || $1 = "--regen" ]]; then
make O=out ARCH=arm64 $DEFCONFIG savedefconfig
cp out/defconfig arch/arm64/configs/$DEFCONFIG
exit
fi

if [[ $1 = "-c" || $1 = "--clean" ]]; then
rm -rf out
fi

mkdir -p out
make O=out ARCH=arm64 $DEFCONFIG

echo -e "\nStarting compilation...\n"
make -j16 O=out ARCH=arm64 \
    LD_LIBRARY_PATH="${TC_DIR}/lib:${LD_LIBRARY_PATH}" \
    CC=clang \
    LD=ld.lld \
    AR=llvm-ar \
    AS=llvm-as \
    NM=llvm-nm \
    OBJCOPY=llvm-objcopy \
    OBJDUMP=llvm-objdump \
    STRIP=llvm-strip \
    CROSS_COMPILE=aarch64-linux-android- \
    CROSS_COMPILE_ARM32=arm-linux-androideabi- \
    CLANG_TRIPLE=aarch64-linux-gnu- Image dtbo.img

env

if [ -f "out/arch/arm64/boot/Image" ]; then
echo -e "\nKernel compiled succesfully! Zipping up...\n"
if [ -d "$AK3_DIR" ]; then
cp -r $AK3_DIR AnyKernel3
elif ! git clone -q https://github.com/ShelbyHell/AnyKernel3; then
echo -e "\nAnyKernel3 repo not found locally and cloning failed! Aborting..."
exit 1
fi
cp out/arch/arm64/boot/Image AnyKernel3
cp out/arch/arm64/boot/dtbo.img AnyKernel3
rm -f *zip
cd AnyKernel3
git checkout master &> /dev/null
zip -r9 "$ZIPNAME" * -x '*.git*' README.md *placeholder
cd ..
rm -rf AnyKernel3
rm -rf out/arch/arm64/boot
echo -e "\nCompleted in $((SECONDS / 60)) minute(s) and $((SECONDS % 60)) second(s) !"
echo "Zip: $ZIPNAME"
if ! [[ $HOSTNAME = "enprytna" && $USER = "endi" ]]; then
curl -F document=@"${ZIPNAME}" -F "caption=${FILE_CAPTION}" "https://api.telegram.org/bot${TELEGRAM_BOT_TOKEN}/sendDocument?chat_id=${TELEGRAM_CHAT_ID}&parse_mode=Markdown"
fi
else
echo -e "\nCompilation failed!"
exit 1
fi
