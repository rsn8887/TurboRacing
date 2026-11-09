#!/bin/sh
# Runs inside the Docker image (see Dockerfile): builds build-android/TurboRacingMayhem-<version>.apk next to the project.
#   RENDER_SCALE=1  render at 640x360 instead of 1280x720 (for slow devices)
set -e
OUT=/work/build-android
mkdir -p "$OUT"
export GRADLE_USER_HOME="$OUT/gradle-home"       # downloaded Gradle plugins, kept between builds

# The APK is signed with this key. It is kept in build-android/ so that new versions install over old ones;
# don't delete it (a different key means uninstalling the old version first).
KEY="$OUT/turboracing.keystore"
if [ ! -f "$KEY" ]; then
  keytool -genkeypair -keystore "$KEY" -storepass turboracing -keypass turboracing -alias turboracing \
          -keyalg RSA -keysize 2048 -validity 20000 -dname "CN=rsn8887" > /dev/null
fi

cd /work/TurboRacingMayhem/android
gradle --no-daemon --console=plain --project-cache-dir "$OUT/.gradle" \
       -Pkeystore="$KEY" -PrenderScale="${RENDER_SCALE:-2}" assembleRelease

# build-android/TurboRacingMayhem-<version>.apk (the version from CMakeLists.txt)
VERSION=$(sed -n 's/^project(TurboRacingMayhem VERSION \([0-9.]*\).*/\1/p' /work/TurboRacingMayhem/CMakeLists.txt)
cp "$OUT/app/outputs/apk/release/app-release.apk" "$OUT/TurboRacingMayhem-$VERSION.apk"
echo "done: build-android/TurboRacingMayhem-$VERSION.apk"
