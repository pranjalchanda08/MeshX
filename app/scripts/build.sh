TARGET=$1

echo "TARGET = $TARGET"
python3 scripts/code_gen.py $TARGET
idf.py -DTARGET="$TARGET" clean build
