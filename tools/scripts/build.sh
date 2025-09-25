TARGET=$1

echo "TARGET = $TARGET"
python3 scripts/code_gen.py $TARGET
APP_PATH=$(pwd)
BUILD_PARAM=""
echo $2
# clean only if $2 is "clean"
if [ "$2" = "clean" ]; then
    echo "Cleaning build..."
    rm -rfv build/*
    BUILD_PARAM="set-target esp32c3 clean"
fi
idf.py -DTARGET="$TARGET" $BUILD_PARAM build
# sudo docker run --rm -v "$PWD:$APP_PATH" -w "$APP_PATH" -u "$UID" -e HOME=/tmp espressif/idf:release-v5.4 idf.py -DTARGET="$TARGET" set-target esp32c3 build
