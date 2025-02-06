//==--- DiagnosticSemaKinds.td - compatibility diagnostics ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Compatibility Diagnostics
//===----------------------------------------------------------------------===//

// Define a C++ compatibility warning.
multiclass CXXCompatWarn<string message, int std_ver> {
    def ext_#NAME :
        ExtWarn<!strconcat(message, " a C++", ""#std_ver,  " extension")>,
        InGroup<!cast<DiagGroup>("CXX"#std_ver)>;

    def warn_compat_#NAME :
        Warning<!strconcat(message, " incompatible with C++ standards before C++", ""#std_ver)>,
        InGroup<!cast<DiagGroup>("CXXPre"#std_ver#"Compat")>,
        DefaultIgnore;
}

multiclass CXX26CompatWarn<string message> : CXXCompatWarn<message, 26>;

let Component = "Compat" in {
let CategoryName = "Compatibility Diagnostic" in {

//===----------------------------------------------------------------------===//
// C++26
//===----------------------------------------------------------------------===//
defm variadic_friends : CXX26CompatWarn<"variadic 'friend' declarations are">;

} // CategoryName
} // Component
