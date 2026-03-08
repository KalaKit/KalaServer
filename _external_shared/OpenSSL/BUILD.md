# clean build

get latest release: https://github.com/openssl/openssl

## windows build

...

## linux build

rm -rf _build && \
make clean && \
\
mkdir -p _build/release && \
\
./Configure linux-x86_64 \
    --prefix=$PWD/_build/release \
    no-tests \
    no-docs \
    no-shared \
    no-apps \
    no-legacy \
    no-comp \
    no-async \
    no-engine \
    no-module && \
\
make -j$(nproc) build_sw && \
make install_sw && \
make clean && \
mv _build/release/lib64/*.a _build/release && \
rm -rf _build/release/lib64 && \
rm -rf _build/release/bin && \
\
mkdir -p _build/debug && \
\
./Configure linux-x86_64 \
    --debug \
    --prefix=$PWD/_build/debug \
    no-tests \
    no-docs \
    no-shared \
    no-apps \
    no-legacy \
    no-comp \
    no-async \
    no-engine \
    no-module && \
\
make -j$(nproc) build_sw && \
make install_sw && \
mv _build/debug/lib64/*.a _build/debug && \
rm -rf _build/debug/lib64 && \
rm -rf _build/debug/bin && \
echo -e "\n##########\n\nFinished building into _build/release and _build/debug.\n\n##########\n"
