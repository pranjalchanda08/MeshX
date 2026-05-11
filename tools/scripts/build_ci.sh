TARGET=$1

echo "TARGET = $TARGET"
APP_PATH=$(pwd)
sudo docker run --rm \
    -v "$PWD:$APP_PATH" -w "$APP_PATH" -u "$UID" \
    -v "$CCACHE_DIR:/tmp/.ccache" \
    -e CCACHE_DIR=/tmp/.ccache \
    -e HOME=/tmp \
    espressif/idf:release-v5.4 idf.py -B build/release -DPROD_NAME="$TARGET" build
