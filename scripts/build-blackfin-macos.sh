#!/bin/bash
# Native macOS ARM64 cross compiler; all products stay in the project.
# Requires Xcode CLT, existing GMP/MPFR/MPC at /opt/homebrew/opt, curl and make.
set -euo pipefail
cd "$(dirname "$0")/.."
project_root=$PWD
task_build="$project_root/.work/bfin-native"
task_prefix="$task_build/install"
mkdir -p "$task_build/downloads" "$task_build/src" "$task_build/binutils-build" "$task_build/gcc-build"
for item in binutils/binutils-2.38.tar.xz gcc/gcc-13.3.0/gcc-13.3.0.tar.xz; do
  name=${item##*/}
  [ -f "$task_build/downloads/$name" ] || curl -fL "https://ftp.gnu.org/gnu/$item" -o "$task_build/downloads/$name"
  [ -f "$task_build/downloads/$name.sig" ] || curl -fL "https://ftp.gnu.org/gnu/$item.sig" -o "$task_build/downloads/$name.sig"
done
curl -fL https://ftp.gnu.org/gnu/gnu-keyring.gpg -o "$task_build/downloads/gnu-keyring.gpg"
[ -d .work/verify-env ] || python3 -m venv .work/verify-env
.work/verify-env/bin/pip install pgpy==0.6.0
.work/verify-env/bin/python scripts/verify-gnu.py > "$task_build/signature-check.txt"
[ -d "$task_build/src/binutils-2.38" ] || tar -xf "$task_build/downloads/binutils-2.38.tar.xz" -C "$task_build/src"
[ -d "$task_build/src/gcc-13.3.0" ] || tar -xf "$task_build/downloads/gcc-13.3.0.tar.xz" -C "$task_build/src"
cd "$task_build/binutils-build"
../src/binutils-2.38/configure --target=bfin-elf --prefix="$task_prefix" --disable-nls --disable-werror --disable-gdb --disable-sim --with-system-zlib > configure.log 2>&1
make -j6 MAKEINFO=true > make.log 2>&1
make MAKEINFO=true install > install.log 2>&1
export PATH="$task_prefix/bin:$PATH"
cd "$task_build/gcc-build"
../src/gcc-13.3.0/configure --target=bfin-elf --prefix="$task_prefix" --disable-nls --disable-werror --enable-languages=c --without-headers --with-newlib --disable-shared --disable-threads --disable-multilib --disable-libssp --disable-libgomp --disable-libquadmath --disable-libatomic --disable-bootstrap --without-isl --with-gmp=/opt/homebrew/opt/gmp --with-mpfr=/opt/homebrew/opt/mpfr --with-mpc=/opt/homebrew/opt/libmpc --with-system-zlib --with-as="$task_prefix/bin/bfin-elf-as" --with-ld="$task_prefix/bin/bfin-elf-ld" > configure.log 2>&1
make -j6 MAKEINFO=true all-gcc > make.log 2>&1
make MAKEINFO=true install-gcc > install.log 2>&1
make -j6 MAKEINFO=true all-target-libgcc > libgcc.log 2>&1
make MAKEINFO=true install-target-libgcc > libgcc-install.log 2>&1
cd "$project_root"
export OP1_BFIN_TOOLCHAIN="$task_prefix"
printf '%s\n' "Toolchain ready: $task_prefix" "Next: sh scripts/setup-sdk.sh; sh scripts/build.sh"
