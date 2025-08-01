// UNSUPPORTED: system-windows

// RUN: %clang -### %s --target=armv6-none-eabi --emit-static-lib 2>&1 \
// RUN:   | FileCheck -check-prefixes=CHECK-STATIC-LIB %s
// CHECK-STATIC-LIB: {{.*}}llvm-ar{{.*}}" "rcsD"

// RUN: %clang %s -### --target=armv6m-none-eabi -o %t.out 2>&1 \
// RUN:     -T semihosted.lds \
// RUN:     -L some/directory/user/asked/for \
// RUN:     --sysroot=%S/Inputs/baremetal_arm \
// RUN:   | FileCheck --check-prefix=CHECK-V6M-C %s
// CHECK-V6M-C:      "-cc1" "-triple" "thumbv6m-unknown-none-eabi"
// CHECK-V6M-C-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-V6M-C-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-V6M-C-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-V6M-C-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}include"
// CHECK-V6M-C-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-V6M-C-NEXT: ld{{(.exe)?}}"
// CHECK-V6M-C-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-V6M-C-SAME: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-V6M-C-SAME: "[[SYSROOT:[^"]+]]{{[/\\]+}}lib{{[/\\]+}}crt0.o"
// CHECK-V6M-C-SAME: "-T" "semihosted.lds" "-Lsome{{[/\\]+}}directory{{[/\\]+}}user{{[/\\]+}}asked{{[/\\]+}}for"
// CHECK-V6M-C-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}lib"
// CHECK-V6M-C-SAME: "{{.*}}.o"
// CHECK-V6M-C-SAME: {{[^"]*}}libclang_rt.builtins.a"
// CHECK-V6M-C-SAME: "-lc"
// CHECK-V6M-C-SAME: "--target2=rel" "-o" "{{.*}}.tmp.out"

// RUN: %clang %s -### --target=armv6m-none-eabi -nostdlibinc -nobuiltininc 2>&1 \
// RUN:     --sysroot=%S/Inputs/baremetal_arm | FileCheck --check-prefix=CHECK-V6M-LIBINC %s
// RUN: %clang %s -### --target=armv6m-none-eabi -nostdinc 2>&1 \
// RUN:     --sysroot=%S/Inputs/baremetal_arm | FileCheck --check-prefix=CHECK-V6M-LIBINC %s
// CHECK-V6M-LIBINC-NOT: "-internal-isystem"

// RUN: %clang %s -### --target=armv6m-none-eabi -o %t.out 2>&1 \
// RUN:     -ccc-install-dir %S/Inputs/basic_baremetal_tree/bin \
// RUN:   | FileCheck --check-prefix=CHECK-V6M-TREE %s
// CHECK-V6M-TREE:      InstalledDir: [[INSTALLED_DIR:.+]]
// CHECK-V6M-TREE:      "-cc1" "-triple" "thumbv6m-unknown-none-eabi"
// CHECK-V6M-TREE-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-V6M-TREE-SAME: "-internal-isystem" "[[INSTALLED_DIR]]{{[/\\]+}}..{{[/\\]+}}include{{[/\\]+}}armv6m-unknown-none-eabi{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-V6M-TREE-SAME: {{^}} "-internal-isystem" "[[INSTALLED_DIR]]{{[/\\]+}}..{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-V6M-TREE-SAME: "-internal-isystem" "[[INSTALLED_DIR]]{{[/\\]+}}..{{[/\\]+}}include{{[/\\]+}}armv6m-unknown-none-eabi"
// CHECK-V6M-TREE-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-V6M-TREE-NEXT: ld{{(.exe)?}}"
// CHECK-V6M-TREE-SAME: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-V6M-TREE-SAME: "[[INSTALLED_DIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}armv6m-unknown-none-eabi{{[/\\]+}}crt0.o"
// CHECK-V6M-TREE-SAME: "-L[[INSTALLED_DIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}armv6m-unknown-none-eabi"
// CHECK-V6M-TREE-SAME "{{.*}}.o"
// CHECK-V6M-TREE-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-V6M-TREE-SAME: "-lc"
// CHECK-V6M-TREE-SAME: "--target2=rel" "-o" "{{.*}}.tmp.out"

// RUN: %clang %s -### --target=armv7m-vendor-none-eabi -rtlib=compiler-rt 2>&1 \
// RUN:     --sysroot=%S/Inputs/baremetal_arm \
// RUN:     -resource-dir=%S/Inputs/resource_dir_with_per_target_subdir \
// RUN:   | FileCheck --check-prefix=CHECK-ARMV7M-PER-TARGET %s
// CHECK-ARMV7M-PER-TARGET: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-ARMV7M-PER-TARGET: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-ARMV7M-PER-TARGET: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-ARMV7M-PER-TARGET: ld{{(.exe)?}}"
// CHECK-ARMV7M-PER-TARGET: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-ARMV7M-PER-TARGET: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-ARMV7M-PER_TARGET: "[[SYSROOT:[^"]+]]{{[/\\]+}}lib{{[/\\]+}}crt0.o"
// CHECK-ARMV7M-PER-TARGET: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}lib"
// CHECK-ARMV7M-PER-TARGET: "-L[[RESOURCE_DIR:[^"]+]]{{[/\\]+}}lib{{[/\\]+}}armv7m-vendor-none-eabi
// CHECK-ARMV7M-PER-TARGET: "{{.*}}.o"
// CHECK-ARMV7M-PER-TARGET: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-ARMV7M-PER-TARGET: "-lc"

// RUN: %clangxx %s -### --target=armv6m-none-eabi 2>&1 \
// RUN:     --sysroot=%S/Inputs/baremetal_arm | FileCheck --check-prefix=CHECK-V6M-DEFAULTCXX %s
// CHECK-V6M-DEFAULTCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-V6M-DEFAULTCXX: ld{{(.exe)?}}"
// CHECK-V6M-DEFAULTCXX: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-V6M-DEFAULTCXX: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-V6M-DEFAULTCXX-SAME: "[[SYSROOT:[^"]+]]{{[/\\]+}}lib{{[/\\]+}}crt0.o"
// CHECK-V6M-DEFAULTCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm{{[/\\]+}}lib"
// CHECK-V6M-DEFAULTCXX-SAME: "{{.*}}.o"
// CHECK-V6M-DEFAULTCXX-SAME: "-lc++"
// CHECK-V6M-DEFAULTCXX-SAME: "-lm"
// CHECK-V6M-DEFAULTCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-V6M-DEFAULTCXX-SAME: "-lc"
// CHECK-V6M-DEFAULTCXX-SAME: "--target2=rel" "-o" "a.out"

// RUN: %clangxx %s -### --target=armv6m-none-eabi -stdlib=libc++ 2>&1 \
// RUN:     --sysroot=%S/Inputs/baremetal_arm | FileCheck --check-prefix=CHECK-V6M-LIBCXX %s
// CHECK-V6M-LIBCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-V6M-LIBCXX-NOT: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}{{[^v].*}}"
// CHECK-V6M-LIBCXX-SAME: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-V6M-LIBCXX: ld{{(.exe)?}}"
// CHECK-V6M-LIBCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-V6M-LIBCXX-SAME: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-V6M-LIBCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm{{[/\\]+}}lib"
// CHECK-V6M-LIBCXX-SAME: "{{.*}}.o"
// CHECK-V6M-LIBCXX-SAME: "-lc++"
// CHECK-V6M-LIBCXX-SAME: "-lm"
// CHECK-V6M-LIBCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-V6M-LIBCXX-SAME: "-lc"
// CHECK-V6M-LIBCXX-SAME: "--target2=rel" "-o" "a.out"

// RUN: %clangxx %s -### --target=armv6m-none-eabi 2>&1 \
// RUN:     --sysroot=%S/Inputs/baremetal_arm \
// RUN:     -stdlib=libstdc++ \
// RUN:   | FileCheck --check-prefix=CHECK-V6M-LIBSTDCXX %s
// CHECK-V6M-LIBSTDCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-V6M-LIBSTDCXX-NOT: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-V6M-LIBSTDCXX-SAME: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}6.0.0"
// CHECK-V6M-LIBSTDCXX: ld{{(.exe)?}}"
// CHECK-V6M-LIBSTDCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-V6M-LIBSTDCXX-SAME: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-V6M-LIBSTDCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm{{[/\\]+}}lib"
// CHECK-V6M-LIBSTDCXX-SAME: "{{.*}}.o"
// CHECK-V6M-LIBSTDCXX-SAME: "-lstdc++" "-lm"
// CHECK-V6M-LIBSTDCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-V6M-LIBSTDCXX-SAME: "-lc"
// CHECK-V6M-LIBSTDCXX-SAME: "--target2=rel" "-o" "a.out"

// RUN: %clangxx %s -### --target=armv6m-none-eabi 2>&1 \
// RUN:     --sysroot=%S/Inputs/baremetal_arm \
// RUN:     -nodefaultlibs \
// RUN:   | FileCheck --check-prefix=CHECK-V6M-NDL %s
// CHECK-V6M-NDL: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-V6M-NDL: ld{{(.exe)?}}"
// CHECK-V6M-NDL: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-V6M-NDL: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-V6M-NDL-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm{{[/\\]+}}lib"

// RUN: rm -rf %T/baremetal_cxx_sysroot
// RUN: mkdir -p %T/baremetal_cxx_sysroot/usr/include/c++/v1
// RUN: %clangxx %s -### 2>&1 \
// RUN:     --target=armv6m-none-eabi \
// RUN:     --sysroot=%T/baremetal_cxx_sysroot \
// RUN:     -stdlib=libc++ \
// RUN:   | FileCheck --check-prefix=CHECK-V6M-LIBCXX-USR %s
// CHECK-V6M-LIBCXX-USR: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-V6M-LIBCXX-USR-NOT: "-internal-isystem" "{{[^"]+}}baremetal_cxx_sysroot{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}{{[^v].*}}"
// CHECK-V6M-LIBCXX-USR-SAME: "-internal-isystem" "{{[^"]+}}baremetal_cxx_sysroot{{[/\\]+}}usr{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-V6M-LIBCXX-USR: "{{[^"]*}}-Bstatic"
// CHECK-V6M-LIBCXX-USR-SAME: "-L{{[^"]*}}{{[/\\]+}}baremetal_cxx_sysroot{{[/\\]+}}lib"
// CHECK-V6M-LIBCXX-USR: "{{.*}}.o"
// CHECK-V6M-LIBCXX-USR-SAME: "-lc++" "-lm"
// CHECK-V6M-LIBCXX-USR-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-V6M-LIBCXX-USR-SAME: "-lc"

// RUN: %clangxx --target=arm-none-eabi -v 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-THREAD-MODEL
// CHECK-THREAD-MODEL: Thread model: posix

// RUN: %clangxx --target=arm-none-eabi -mthread-model single -v 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-THREAD-MODEL-SINGLE
// CHECK-THREAD-MODEL-SINGLE: Thread model: single

// RUN: %clangxx --target=arm-none-eabi -mthread-model posix -v 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-THREAD-MODEL-POSIX
// CHECK-THREAD-MODEL-POSIX: Thread model: posix

// RUN: %clangxx -### --target=arm-none-eabi -nostartfiles -v %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-NOSTARTFILES
// CHECK-NOSTARTFILES-NOT: "crt0.o"

// RUN: %clang -### --target=arm-none-eabi -rtlib=libgcc --unwindlib=libgcc -v %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-RTLIB-GCC
// CHECK-RTLIB-GCC: -lgcc

// RUN: %clang -### --target=arm-none-eabi -v %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CHECK-SYSROOT-INC
// CHECK-SYSROOT-INC-NOT: "-internal-isystem" "include"

// RUN: %clang -### %s --target=armebv7-none-eabi --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-ARMV7EB %s
// CHECK-ARMV7EB: "{{.*}}ld{{(.exe)?}}"
// CHECK-ARMV7EB: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-ARMV7EB: "-Bstatic" "-m" "armelfb" "--be8" "-EB"

// RUN: %clang -### %s --target=armv7-none-eabi -mbig-endian --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-ARMV7EB %s

// RUN: %clang -### %s --target=armebv7-none-eabi -mbig-endian --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-ARMV7EB %s

// RUN: %clang -### %s --target=armv7-none-eabi --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-ARMV7EL %s
// CHECK-ARMV7EL: "{{.*}}ld{{(.exe)?}}"
// CHECK-ARMV7EL: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-ARMV7EL: "-Bstatic" "-m" "armelf" "-EL"
// CHECK-ARMV7EL-NOT: "--be8"

// RUN: %clang -### %s --target=armebv7-none-eabi -mlittle-endian --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-ARMV7EL %s

// RUN: %clang -### %s --target=armv7-none-eabi -mlittle-endian --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-ARMV7EL %s

// RUN: %clang -### %s --target=aarch64_be-none-elf --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-AARCH64BE %s
// CHECK-AARCH64BE: "{{.*}}ld{{(.exe)?}}"
// CHECK-AARCH64BE: sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-AARCH64BE: "-Bstatic" "-m" "aarch64elfb" "-EB"
// CHECK-AARCH64BE-NOT: "--be8"

// RUN: %clang -### %s --target=aarch64-none-elf -mbig-endian --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-AARCH64BE %s

// RUN: %clang -### %s --target=aarch64_be-none-elf -mbig-endian --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-AARCH64BE %s

// RUN: %clang -### %s --target=aarch64-none-elf --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-AARCH64LE %s
// CHECK-AARCH64LE: "{{.*}}ld{{(.exe)?}}"
// CHECK-AARCH64LE: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}baremetal_arm"
// CHECK-AARCH64LE: "-Bstatic" "-m" "aarch64elf" "-EL"
// CHECK-AARCH64LE-NOT: "--be8"

// RUN: %clang -### %s --target=aarch64_be-none-elf -mlittle-endian --sysroot=%S/Inputs/baremetal_arm 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-AARCH64LE %s

// RUN: %clang -no-canonical-prefixes %s -### --target=aarch64-none-elf 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-AARCH64-NO-HOST-INC %s
// Verify that the bare metal driver does not include any host system paths:
// CHECK-AARCH64-NO-HOST-INC: InstalledDir: [[INSTALLEDDIR:.+]]
// CHECK-AARCH64-NO-HOST-INC: "-resource-dir" "[[RESOURCE:[^"]+]]"
// CHECK-AARCH64-NO-HOST-INC-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-AARCH64-NO-HOST-INC-SAME: "-internal-isystem" "[[RESOURCE]]{{[/\\]+}}include"
// CHECK-AARCH64-NO-HOST-INC-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include"

// RUN: %clang -no-canonical-prefixes %s -### --target=riscv32-unknown-elf 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-RISCV32-NO-HOST-INC %s
// CHECK-RISCV32-NO-HOST-INC: InstalledDir: [[INSTALLEDDIR:.+]]
// CHECK-RISCV32-NO-HOST-INC: "-resource-dir" "[[RESOURCE:[^"]+]]"
// CHECK-RISCV32-NO-HOST-INC-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RISCV32-NO-HOST-INC-SAME: "-internal-isystem" "[[RESOURCE]]{{[/\\]+}}include"
// CHECK-RISCV32-NO-HOST-INC-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include"

// RUN: %clang -no-canonical-prefixes %s -### --target=riscv64-unknown-elf 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-RISCV64-NO-HOST-INC %s
// CHECK-RISCV64-NO-HOST-INC: InstalledDir: [[INSTALLEDDIR:.+]]
// CHECK-RISCV64-NO-HOST-INC: "-resource-dir" "[[RESOURCE:[^"]+]]"
// CHECK-RISCV64-NO-HOST-INC-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RISCV64-NO-HOST-INC-SAME: "-internal-isystem" "[[RESOURCE]]{{[/\\]+}}include"
// CHECK-RISCV64-NO-HOST-INC-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include"

// RUN: %clang %s -### --target=riscv64-unknown-elf -o %t.out -L some/directory/user/asked/for \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-RV64 %s
// CHECK-RV64:      "-cc1" "-triple" "riscv64-unknown-unknown-elf"
// CHECK-RV64-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV64-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-RV64-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV64-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}include"
// CHECK-RV64-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-RV64-NEXT: ld{{(.exe)?}}"
// CHECK-RV64-SAME: "--sysroot={{.*}}/Inputs/basic_riscv64_tree/riscv64-unknown-elf"
// CHECK-RV64-SAME: "-Bstatic" "-m" "elf64lriscv" "-X"
// CHECK-RV64-SAME: "-Lsome{{[/\\]+}}directory{{[/\\]+}}user{{[/\\]+}}asked{{[/\\]+}}for"
// CHECK-RV64-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}lib"
// CHECK-RV64-SAME:"{{.*}}.o"
// CHECK-RV64-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV64-SAME: "-lc"
// CHECK-RV64-SAME: "-o" "{{.*}}.tmp.out"

// RUN: %clangxx %s -### --target=riscv64-unknown-elf 2>&1 \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-DEFAULTCXX %s
// CHECK-RV64-DEFAULTCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV64-DEFAULTCXX: ld{{(.exe)?}}"
// CHECK-RV64-DEFAULTCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf"
// CHECK-RV64-DEFAULTCXX-SAME: "-Bstatic" "-m" "elf64lriscv" "-X"
// CHECK-RV64-DEFAULTCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf{{[/\\]+}}lib"
// CHECK-RV64-DEFAULTCXX-SAME:"{{.*}}.o"
// CHECK-RV64-DEFAULTCXX-SAME: "-lc++" "-lm"
// CHECK-RV64-DEFAULTCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV64-DEFAULTCXX-SAME: "-lc"
// CHECK-RV64-DEFAULTCXX-SAME: "-o" "a.out"

// RUN: %clangxx %s -### --target=riscv64-unknown-elf 2>&1 \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:     -stdlib=libc++ \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-LIBCXX %s
// CHECK-RV64-LIBCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV64-LIBCXX-NOT: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}{{[^v].*}}"
// CHECK-RV64-LIBCXX-SAME: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV64-LIBCXX: ld{{(.exe)?}}"
// CHECK-RV64-LIBCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf"
// CHECK-RV64-LIBCXX-SAME: "-Bstatic" "-m" "elf64lriscv" "-X"
// CHECK-RV64-LIBCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf{{[/\\]+}}lib"
// CHECK-RV64-LIBCXX-SAME:"{{.*}}.o"
// CHECK-RV64-LIBCXX-SAME: "-lc++" "-lm"
// CHECK-RV64-LIBCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV64-LIBCXX-SAME: "-lc"
// CHECK-RV64-LIBCXX-SAME: "-o" "a.out"

// RUN: %clangxx %s -### 2>&1 --target=riscv64-unknown-elf \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:     -stdlib=libstdc++ \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-LIBSTDCXX %s
// CHECK-RV64-LIBSTDCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV64-LIBSTDCXX-NOT: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV64-LIBSTDCXX-SAME: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}8.0.1"
// CHECK-RV64-LIBSTDCXX: ld{{(.exe)?}}"
// CHECK-RV64-LIBSTDCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf"
// CHECK-RV64-LIBSTDCXX-SAME: "-Bstatic" "-m" "elf64lriscv" "-X"
// CHECK-RV64-LIBSTDCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf{{[/\\]+}}lib"
// CHECK-RV64-LIBSTDCXX-SAME: "{{.*}}.o"
// CHECK-RV64-LIBSTDCXX-SAME: "-lstdc++" "-lm"
// CHECK-RV64-LIBSTDCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV64-LIBSTDCXX-SAME: "-lc"
// CHECK-RV64-LIBSTDCXX-SAME: "-o" "a.out"

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     -L some/directory/user/asked/for \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32 %s
// CHECK-RV32:      "-cc1" "-triple" "riscv32-unknown-unknown-elf"
// CHECK-RV32-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-RV32-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV32-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}include"
// CHECK-RV32-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-RV32-NEXT: ld{{(.exe)?}}"
// CHECK-RV32-SAME: "--sysroot={{.*}}/Inputs/basic_riscv32_tree/riscv32-unknown-elf"
// CHECK-RV32-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32-SAME: "-Lsome{{[/\\]+}}directory{{[/\\]+}}user{{[/\\]+}}asked{{[/\\]+}}for"
// CHECK-RV32-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}lib"
// CHECK-RV32-SAME: "{{.*}}.o"
// CHECK-RV32-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV32-SAME: "-lc"
// CHECK-RV32-SAME: "-o" "a.out"

// RUN: %clangxx %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32-DEFAULTCXX %s
// CHECK-RV32-DEFAULTCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32-DEFAULTCXX: ld{{(.exe)?}}"
// CHECK-RV32-DEFAULTCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf"
// CHECK-RV32-DEFAULTCXX-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32-DEFAULTCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf{{[/\\]+}}lib"
// CHECK-RV32-DEFAULTCXX-SAME: "{{.*}}.o"
// CHECK-RV32-DEFAULTCXX-SAME: "-lc++" "-lm"
// CHECK-RV32-DEFAULTCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV32-DEFAULTCXX-SAME: "-lc"
// CHECK-RV32-DEFAULTCXX-SAME: "-o" "a.out"

// RUN: %clangxx %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:     -stdlib=libc++ \
// RUN:   | FileCheck --check-prefix=CHECK-RV32-LIBCXX %s
// CHECK-RV32-LIBCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32-LIBCXX-NOT: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}{{[^v].*}}"
// CHECK-RV32-LIBCXX-SAME: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV32-LIBCXX: ld{{(.exe)?}}"
// CHECK-RV32-LIBCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf"
// CHECK-RV32-LIBCXX-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32-LIBCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf{{[/\\]+}}lib"
// CHECK-RV32-LIBCXX-SAME: "{{.*}}.o"
// CHECK-RV32-LIBCXX-SAME: "-lc++" "-lm"
// CHECK-RV32-LIBCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV32-LIBCXX-SAME: "-o" "a.out"

// RUN: %clangxx %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:     -stdlib=libstdc++ \
// RUN:   | FileCheck --check-prefix=CHECK-RV32-LIBSTDCXX %s
// CHECK-RV32-LIBSTDCXX: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32-LIBSTDCXX-NOT: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV32-LIBSTDCXX-SAME: "-internal-isystem" "{{[^"]+}}{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}8.0.1"
// CHECK-RV32-LIBSTDCXX: ld{{(.exe)?}}"
// CHECK-RV32-LIBSTDCXX-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf"
// CHECK-RV32-LIBSTDCXX-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32-LIBSTDCXX-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf{{[/\\]+}}lib"
// CHECK-RV32-LIBSTDCXX-SAME: "{{.*}}.o"
// CHECK-RV32-LIBSTDCXX-SAME: "-lstdc++" "-lm"
// CHECK-RV32-LIBSTDCXX-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-RV32-LIBSTDCXX-SAME: "-lc"
// CHECK-RV32-LIBSTDCXX-SAME: "-o" "a.out"

// RUN: %clang %s -### 2>&1 --target=riscv64-unknown-elf \
// RUN:     -nostdlibinc -nobuiltininc \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-LIBINC %s

// RUN: %clang %s -### 2>&1 --target=riscv64-unknown-elf -nostdinc \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-LIBINC %s
// CHECK-RV64-LIBINC-NOT: "-internal-isystem"

// RUN: %clangxx %s -### 2>&1 --target=riscv64-unknown-elf \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:     -nodefaultlibs \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-NDL %s
// CHECK-RV64-NDL: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV64-NDL: ld{{(.exe)?}}"
// CHECK-RV64-NDL-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf"
// CHECK-RV64-NDL-SAME: "-Bstatic" "-m" "elf64lriscv" "-X"
// CHECK-RV64-NDL-SAME: "-L{{[^"]*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf{{[/\\]+}}lib"

// RUN: %clang %s -### 2>&1 --target=riscv64-unknown-elf \
// RUN:     -march=rv64imafdc -mabi=lp64d \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV64FD %s

// RUN: %clang %s -### 2>&1 --target=riscv64-unknown-elf \
// RUN:     -march=rv64gc -mabi=lp64d \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV64FD %s

// CHECK-RV64FD:      "-cc1" "-triple" "riscv64-unknown-unknown-elf"
// CHECK-RV64FD-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV64FD-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-RV64FD-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv64imafdc{{[/\\]+}}lp64d{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV64FD-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv64imafdc{{[/\\]+}}lp64d{{[/\\]+}}include"
// CHECK-RV64FD-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-RV64FD-NEXT: ld{{(.exe)?}}"
// CHECK-RV64FD-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv64_tree{{[/\\]+}}riscv64-unknown-elf"
// CHECK-RV64FD-SAME: "-Bstatic" "-m" "elf64lriscv" "-X"
// CHECK-RV64FD-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}rv64imafdc{{[/\\]+}}lp64d{{[/\\]+}}lib"

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     -march=rv32i -mabi=ilp32 \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32I %s

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     -march=rv32ic -mabi=ilp32 \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32I %s

// CHECK-RV32I:      "-cc1" "-triple" "riscv32-unknown-unknown-elf"
// CHECK-RV32I-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32I-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-RV32I-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32i{{[/\\]+}}ilp32{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV32I-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32i{{[/\\]+}}ilp32{{[/\\]+}}include"
// CHECK-RV32I-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-RV32I-NEXT: ld{{(.exe)?}}"
// CHECK-RV32I-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf"
// CHECK-RV32I-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32I-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}rv32i{{[/\\]+}}ilp32{{[/\\]+}}lib"

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     -march=rv32im -mabi=ilp32 \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32IM %s

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     -march=rv32imc -mabi=ilp32 \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32IM %s

// CHECK-RV32IM:      "-cc1" "-triple" "riscv32-unknown-unknown-elf"
// CHECK-RV32IM-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32IM-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-RV32IM-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32im{{[/\\]+}}ilp32{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV32IM-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32im{{[/\\]+}}ilp32{{[/\\]+}}include"
// CHECK-RV32IM-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-RV32IM-NEXT: ld{{(.exe)?}}"
// CHECK-RV32IM-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf"
// CHECK-RV32IM-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32IM-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}rv32im{{[/\\]+}}ilp32{{[/\\]+}}lib"

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf \
// RUN:     -march=rv32iac -mabi=ilp32 \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32IAC %s

// CHECK-RV32IAC:      "-cc1" "-triple" "riscv32-unknown-unknown-elf"
// CHECK-RV32IAC-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32IAC-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-RV32IAC-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32iac{{[/\\]+}}ilp32{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV32IAC-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32iac{{[/\\]+}}ilp32{{[/\\]+}}include"
// CHECK-RV32IAC-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-RV32IAC-NEXT: ld{{(.exe)?}}"
// CHECK-RV32IAC-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf"
// CHECK-RV32IAC-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32IAC-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}rv32iac{{[/\\]+}}ilp32{{[/\\]+}}lib"

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf -march=rv32imafc -mabi=ilp32f \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32IMAFC %s

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf -march=rv32imafdc -mabi=ilp32f \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32IMAFC %s

// RUN: %clang %s -### 2>&1 --target=riscv32-unknown-elf -march=rv32gc -mabi=ilp32f \
// RUN:     --sysroot=%S/Inputs/basic_riscv32_tree/riscv32-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV32IMAFC %s

// CHECK-RV32IMAFC:      "-cc1" "-triple" "riscv32-unknown-unknown-elf"
// CHECK-RV32IMAFC-SAME: "-resource-dir" "[[RESOURCE_DIR:[^"]+]]"
// CHECK-RV32IMAFC-SAME: "-isysroot" "[[SYSROOT:[^"]*]]"
// CHECK-RV32IMAFC-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32imafc{{[/\\]+}}ilp32f{{[/\\]+}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-RV32IMAFC-SAME: "-internal-isystem" "[[SYSROOT]]{{[/\\]+}}rv32imafc{{[/\\]+}}ilp32f{{[/\\]+}}include"
// CHECK-RV32IMAFC-SAME: "-x" "c++" "{{.*}}baremetal.cpp"
// CHECK-RV32IMAFC-NEXT: ld{{(.exe)?}}"
// CHECK-RV32IMAFC-SAME: "--sysroot={{.*}}{{[/\\]+}}Inputs{{[/\\]+}}basic_riscv32_tree{{[/\\]+}}riscv32-unknown-elf"
// CHECK-RV32IMAFC-SAME: "-Bstatic" "-m" "elf32lriscv" "-X"
// CHECK-RV32IMAFC-SAME: "-L[[SYSROOT:[^"]+]]{{[/\\]+}}rv32imafc{{[/\\]+}}ilp32f{{[/\\]+}}lib"

// RUN: %clang -no-canonical-prefixes %s -### --target=powerpc-unknown-eabi 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-PPCEABI %s
// CHECK-PPCEABI: InstalledDir: [[INSTALLEDDIR:.+]]
// CHECK-PPCEABI: "-nostdsysteminc"
// CHECK-PPCEABI-SAME: "-resource-dir" "[[RESOURCE:[^"]+]]"
// CHECK-PPCEABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-PPCEABI-SAME: "-internal-isystem" "[[RESOURCE]]{{[/\\]+}}include"
// CHECK-PPCEABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include"
// CHECK-PPCEABI-NEXT: ld{{(.exe)?}}" "-Bstatic"
// CHECK-PPCEABI-SAME: "-L[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}lib"
// CHECK-PPCEABI-SAME:"{{.*}}.o"
// CHECK-PPCEABI-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-PPCEABI-SAME: "-lc"
// CHECK-PPCEABI-SAME: "-o" "a.out"

// RUN: %clang -no-canonical-prefixes %s -### --target=powerpc64-unknown-eabi 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-PPC64EABI %s
// CHECK-PPC64EABI: InstalledDir: [[INSTALLEDDIR:.+]]
// CHECK-PPC64EABI: "-nostdsysteminc"
// CHECK-PPC64EABI-SAME: "-resource-dir" "[[RESOURCE:[^"]+]]"
// CHECK-PPC64EABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-PPC64EABI-SAME: "-internal-isystem" "[[RESOURCE]]{{[/\\]+}}include"
// CHECK-PPC64EABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include"
// CHECK-PPC64EABI-NEXT: ld{{(.exe)?}}" "-Bstatic"
// CHECK-PPC64EABI-SAME: "-L[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}lib"
// CHECK-PPC64EABI-SAME:"{{.*}}.o"
// CHECK-PPC64EABI-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-PPC64EABI-SAME: "-lc"
// CHECK-PPC64EABI-SAME: "-o" "a.out"

// RUN: %clang -no-canonical-prefixes %s -### --target=powerpcle-unknown-eabi 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-PPCLEEABI %s
// CHECK-PPCLEEABI: InstalledDir: [[INSTALLEDDIR:.+]]
// CHECK-PPCLEEABI: "-nostdsysteminc"
// CHECK-PPCLEEABI-SAME: "-resource-dir" "[[RESOURCE:[^"]+]]"
// CHECK-PPCLEEABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-PPCLEEABI-SAME: "-internal-isystem" "[[RESOURCE]]{{[/\\]+}}include"
// CHECK-PPCLEEABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include"
// CHECK-PPCLEEABI-NEXT: ld{{(.exe)?}}" "-Bstatic"
// CHECK-PPCLEEABI-SAME: "-L[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}lib"
// CHECK-PPCLEEABI-SAME:"{{.*}}.o"
// CHECK-PPCLEEABI-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-PPCLEEABI-SAME: "-lc"
// CHECK-PPCLEEABI-SAME: "-o" "a.out"

// RUN: %clang -no-canonical-prefixes %s -### --target=powerpc64le-unknown-eabi 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-PPC64LEEABI %s
// CHECK-PPC64LEEABI: InstalledDir: [[INSTALLEDDIR:.+]]
// CHECK-PPC64LEEABI: "-nostdsysteminc"
// CHECK-PPC64LEEABI-SAME: "-resource-dir" "[[RESOURCE:[^"]+]]"
// CHECK-PPC64LEEABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include{{[/\\]+}}c++{{[/\\]+}}v1"
// CHECK-PPC64LEEABI-SAME: "-internal-isystem" "[[RESOURCE]]{{[/\\]+}}include"
// CHECK-PPC64LEEABI-SAME: "-internal-isystem" "[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}include"
// CHECK-PPC64LEEABI-NEXT: ld{{(.exe)?}}" "-Bstatic"
// CHECK-PPC64LEEABI-SAME: "-L[[INSTALLEDDIR]]{{[/\\]+}}..{{[/\\]+}}lib{{[/\\]+}}clang-runtimes{{[/\\]+[^"]*}}lib"
// CHECK-PPC64LEEABI-SAME:"{{.*}}.o"
// CHECK-PPC64LEEABI-SAME: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-PPC64LEEABI-SAME: "-lc"
// CHECK-PPC64LEEABI-SAME: "-o" "a.out"

// Check that compiler-rt library without the arch filename suffix will
// be used if present.
// RUN: rm -rf %T/baremetal_clang_rt_noarch
// RUN: mkdir -p %T/baremetal_clang_rt_noarch/lib
// RUN: touch %T/baremetal_clang_rt_noarch/lib/libclang_rt.builtins.a
// RUN: %clang %s -### 2>&1 \
// RUN:     --target=armv6m-none-eabi \
// RUN:     --sysroot=%T/baremetal_clang_rt_noarch \
// RUN:   | FileCheck --check-prefix=CHECK-CLANGRT-NOARCH %s
// CHECK-CLANGRT-NOARCH: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-CLANGRT-NOARCH-NOT: "{{[^"]*}}libclang_rt.builtins.a"

// Check that compiler-rt library with the arch filename suffix will be
// used if present.
// RUN: rm -rf %T/baremetal_clang_rt_arch
// RUN: mkdir -p %T/baremetal_clang_rt_arch/lib
// RUN: touch %T/baremetal_clang_rt_arch/lib/libclang_rt.builtins-armv6m.a
// RUN: %clang %s -### 2>&1 \
// RUN:     --target=armv6m-none-eabi \
// RUN:     --sysroot=%T/baremetal_clang_rt_arch \
// RUN:   | FileCheck --check-prefix=CHECK-CLANGRT-ARCH %s
// CHECK-CLANGRT-ARCH: "{{[^"]*}}libclang_rt.builtins.a"
// CHECK-CLANGRT-ARCH-NOT: "{{[^"]*}}libclang_rt.builtins.a"

// Check that "--no-relax" is forwarded to the linker for RISC-V.
// RUN: %clang %s -### 2>&1 --target=riscv64-unknown-elf -nostdinc -mno-relax \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-NORELAX %s
// CHECK-RV64-NORELAX: "--no-relax"

// Check that "--no-relax" is not forwarded to the linker for RISC-V.
// RUN: %clang %s -### 2>&1 --target=riscv64-unknown-elf -nostdinc \
// RUN:     --sysroot=%S/Inputs/basic_riscv64_tree/riscv64-unknown-elf \
// RUN:   | FileCheck --check-prefix=CHECK-RV64-RELAX %s
// CHECK-RV64-RELAX-NOT: "--no-relax"
