//==--- SRCCAttrs.td - attribute definitions for SRCC ---------------------===//
//
// MIT License
//
// Copyright (c) Sirraide
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//===----------------------------------------------------------------------===//

def SRCCDiagnosePointerComparisonDocs : Documentation {
  let Category = DocCatType;
  let Heading = "srcc_diagnose_pointer_comparison";
  let Content = [{
This attribute, when applied to a declaration of some type 'T', instructs Clang to
diagnose any pointer comparisons involving a 'T*'.
  }];
}

def SRCCDiagnosePointerComparison : InheritableAttr {
    let Spellings = [Clang<"srcc_diagnose_pointer_comparison">];
    let Subjects = SubjectList<[Record]>;
    let Documentation = [SRCCDiagnosePointerComparisonDocs];
}
