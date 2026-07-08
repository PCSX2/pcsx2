#!/bin/bash

# Test that the prefab binary exists
if hash prefab 2>/dev/null; then
  echo "Prefab is installed"
else
  echo "Prefab binary not found. See https://github.com/google/prefab for install instructions"
  exit 1
fi

# Get the version string from the source
major=$(grep "#define OBOE_VERSION_MAJOR" include/oboe/Version.h | cut -d' ' -f3)
minor=$(grep "#define OBOE_VERSION_MINOR" include/oboe/Version.h | cut -d' ' -f3)
patch=$(grep "#define OBOE_VERSION_PATCH" include/oboe/Version.h | cut -d' ' -f3)
version=$major"."$minor"."$patch

echo "Building libraries for Oboe version "$version
./build_all_android.sh

mkdir -p build/prefab
cp -R prefab/* build/prefab

ABIS=("x86" "x86_64" "arm64-v8a" "armeabi-v7a")

pushd build/prefab

  # Remove .DS_Store files as these will cause the prefab verification to fail
  find . -name ".DS_Store" -delete

  # Write the version number into the various metadata files
  mv oboe-VERSION oboe-$version
  mv oboe-VERSION.pom oboe-$version.pom
  sed -i '' -e "s/VERSION/${version}/g" oboe-$version.pom oboe-$version/prefab/prefab.json

  # Copy the headers
  cp -R ../../include oboe-$version/prefab/modules/oboe/

  # Copy the libraries
  for abi in ${ABIS[@]}
  do
    echo "Copying the ${abi} library"
    cp -v "../${abi}/liboboe.so" "oboe-${version}/prefab/modules/oboe/libs/android.${abi}/"
  done

  # Verify the prefab packages
  for abi in ${ABIS[@]}
  do

    prefab --build-system cmake --platform android --os-version 29 \
        --stl c++_shared --ndk-version 21 --abi ${abi} \
        --output prefab-output-tmp $(pwd)/oboe-${version}/prefab

    result=$?; if [[ $result == 0 ]]; then
      echo "${abi} package verified"
    else
      echo "${abi} package verification failed"
      exit 1
    fi
  done

  # Zip into an AAR and move into parent dir
  pushd oboe-${version}
    zip -r oboe-${version}.aar . 2>/dev/null;
    zip -Tv oboe-${version}.aar 2>/dev/null;

    # Verify that the aar contents are correct (see output below to verify)
    result=$?; if [[ $result == 0 ]]; then
      echo "AAR verified"
    else
      echo "AAR verification failed"
      exit 1
    fi

    mv oboe-${version}.aar ..
  popd

  # Zip the .aar and .pom files into a maven package
  zip oboe-${version}.zip oboe-${version}.* 2>/dev/null;
  zip -Tv oboe-${version}.zip 2>/dev/null;

  # Verify that the zip contents are correct (see output below to verify)
  result=$?; if [[ $result == 0 ]]; then
    echo "Zip verified"
  else
    echo "Zip verification failed"
    exit 1
  fi
popd

echo "Prefab zip ready for deployment: ./build/prefab/oboe-${version}.zip"
