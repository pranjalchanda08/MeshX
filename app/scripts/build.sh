TARGET=$1

echo "TARGET = $TARGET"
python3 scripts/code_gen.py $TARGET
APP_PATH=$(pwd)
sudo docker run --rm -v "$PWD:$APP_PATH" -w "$APP_PATH" -u "$UID" -e HOME=/tmp espressif/idf:release-v5.4 idf.py -DTARGET="$TARGET" set-target esp32c3 build
# idf.py -DTARGET="$TARGET" set-target esp32c3 clean build
