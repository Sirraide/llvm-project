//===- NVVMDialect.cpp - NVVM IR Ops and Dialect registration -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the types and operation details for the NVVM IR dialect in
// MLIR, and the LLVM IR dialect.  It also registers the dialect.
//
// The NVVM dialect only contains GPU specific additions on top of the general
// LLVM dialect.
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/LLVMIR/NVVMDialect.h"

#include "mlir/Conversion/ConvertToLLVM/ToLLVMInterface.h"
#include "mlir/Dialect/GPU/IR/CompilationInterfaces.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/Types.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/TypeSwitch.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <optional>
#include <string>

using namespace mlir;
using namespace NVVM;

#include "mlir/Dialect/LLVMIR/NVVMOpsDialect.cpp.inc"
#include "mlir/Dialect/LLVMIR/NVVMOpsEnums.cpp.inc"

//===----------------------------------------------------------------------===//
// Verifier methods
//===----------------------------------------------------------------------===//

// This verifier is shared among the following Ops:
// CpAsyncBulkTensorGlobalToSharedClusterOp (TMA Load)
// CpAsyncBulkTensorPrefetchOp (TMA Prefetch)
// CpAsyncBulkTensorReduceOp (TMA Store-Reduce)
static LogicalResult cpAsyncBulkTensorCommonVerifier(size_t tensorDims,
                                                     bool isIm2Col,
                                                     size_t numIm2ColOffsets,
                                                     Location loc) {
  if (tensorDims < 1 || tensorDims > 5)
    return emitError(loc, "expects coordinates between 1 to 5 dimension");

  // For Im2Col mode, there are two constraints:
  if (isIm2Col) {
    // 1. Tensor must always be at least 3-d.
    if (tensorDims < 3)
      return emitError(
          loc,
          "to use im2col mode, the tensor has to be at least 3-dimensional");
    // 2. When there are Im2ColOffsets, they must be (Dims - 2) in number.
    if (numIm2ColOffsets && (tensorDims != (numIm2ColOffsets + 2)))
      return emitError(
          loc, "im2col offsets must be 2 less than number of coordinates");
  }
  return success();
}

LogicalResult CpAsyncBulkTensorGlobalToSharedClusterOp::verify() {
  size_t numIm2ColOffsets = getIm2colOffsets().size();
  bool isIm2Col = numIm2ColOffsets > 0;
  return cpAsyncBulkTensorCommonVerifier(getCoordinates().size(), isIm2Col,
                                         numIm2ColOffsets, getLoc());
}

LogicalResult CpAsyncBulkTensorSharedCTAToGlobalOp::verify() {
  if (getCoordinates().size() > 5)
    return emitError("Maximum 5 coordinates and dimension is supported.");
  return success();
}

LogicalResult CpAsyncOp::verify() {
  if (getModifier() != LoadCacheModifierKind::CG &&
      getModifier() != LoadCacheModifierKind::CA)
    return emitError("Only CG and CA cache modifiers are supported.");
  if (getSize() != 4 && getSize() != 8 && getSize() != 16)
    return emitError("expected byte size to be either 4, 8 or 16.");
  if (getModifier() == LoadCacheModifierKind::CG && getSize() != 16)
    return emitError("CG cache modifier is only support for 16 bytes copy.");
  return success();
}

LogicalResult CpAsyncBulkTensorPrefetchOp::verify() {
  size_t numIm2ColOffsets = getIm2colOffsets().size();
  bool isIm2Col = numIm2ColOffsets > 0;
  return cpAsyncBulkTensorCommonVerifier(getCoordinates().size(), isIm2Col,
                                         numIm2ColOffsets, getLoc());
}

LogicalResult CpAsyncBulkTensorReduceOp::verify() {
  bool isIm2Col = (getMode() == TMAStoreMode::IM2COL);
  return cpAsyncBulkTensorCommonVerifier(getCoordinates().size(), isIm2Col, 0,
                                         getLoc());
}

LogicalResult ConvertFloatToTF32Op::verify() {
  using RndMode = NVVM::FPRoundingMode;
  switch (getRnd()) {
  case RndMode::RNA:
    if (getRelu())
      return emitError("Relu not supported with rna rounding mode.");
    break;
  case RndMode::RN:
  case RndMode::RZ:
    break;
  default:
    return emitError(
        "Only {rn,rz,rna} rounding modes supported for ConvertFloatToTF32Op.");
  }
  return success();
}

LogicalResult ConvertF32x2ToF8x2Op::verify() {
  using RndMode = NVVM::FPRoundingMode;
  using SatMode = NVVM::SaturationMode;

  bool isRoundingModeRN = getRnd() == RndMode::RN;
  bool isRoundingModeRZ = getRnd() == RndMode::RZ;
  bool isRoundingModeRP = getRnd() == RndMode::RP;
  bool isSatFinite = getSat() == SatMode::SATFINITE;

  bool hasRelu = getRelu();

  switch (getType()) {
  case ConvertFP8Type::E4M3:
  case ConvertFP8Type::E5M2:
    if (!isRoundingModeRN)
      return emitOpError("Only RN rounding mode is supported for conversions "
                         "from f32x2 to .e4m3x2 or .e5m2x2 types");
    if (!isSatFinite)
      return emitOpError("Only SATFINITE saturation mode is supported for "
                         "conversions from f32x2 to .e4m3x2 or .e5m2x2 types");
    break;
  case ConvertFP8Type::UE8M0:
    if (!(isRoundingModeRZ || isRoundingModeRP))
      return emitOpError("Only RZ or RP rounding modes are supported for "
                         "conversions from f32x2 to .ue8m0x2 type");
    if (hasRelu)
      return emitOpError("relu not supported for conversions to .ue8m0x2 type");
    break;
  }
  return success();
}

LogicalResult ConvertF16x2ToF8x2Op::verify() {
  if (getType() == ConvertFP8Type::UE8M0)
    return emitOpError("Only .e4m3 or .e5m2 types are supported for "
                       "conversions from f16x2 to f8x2.");

  return success();
}

LogicalResult ConvertBF16x2ToF8x2Op::verify() {
  using RndMode = NVVM::FPRoundingMode;

  if (getType() != ConvertFP8Type::UE8M0)
    return emitOpError(
        "Only .ue8m0 type is supported for conversions from bf16x2 to f8x2.");

  auto rnd = getRnd();
  if (!(rnd == RndMode::RZ || rnd == RndMode::RP))
    return emitOpError("Only RZ and RP rounding modes are supported for "
                       "conversions from bf16x2 to f8x2.");

  return success();
}

LogicalResult BulkStoreOp::verify() {
  if (getInitVal() != 0)
    return emitOpError("only 0 is supported for initVal, got ") << getInitVal();
  return success();
}

// Given the element type of an operand and whether or not it is an accumulator,
// this function returns the PTX type (`NVVM::MMATypes`) that corresponds to the
// operand's element type.
std::optional<mlir::NVVM::MMATypes>
MmaOp::inferOperandMMAType(Type operandElType, bool isAccumulator) {
  auto half2Type =
      VectorType::get(2, Float16Type::get(operandElType.getContext()));
  if (operandElType.isF64())
    return NVVM::MMATypes::f64;
  if (operandElType.isF16() || operandElType == half2Type)
    return NVVM::MMATypes::f16;
  if (operandElType.isF32() && isAccumulator)
    return NVVM::MMATypes::f32;
  if (operandElType.isF32() && !isAccumulator)
    return NVVM::MMATypes::tf32;
  if (llvm::isa<IntegerType>(operandElType)) {
    if (isAccumulator)
      return NVVM::MMATypes::s32;
    return std::nullopt;
  }

  if (auto structType = llvm::dyn_cast<LLVM::LLVMStructType>(operandElType)) {
    if (structType.getBody().empty())
      return std::nullopt;
    return inferOperandMMAType(structType.getBody()[0], isAccumulator);
  }

  return std::nullopt;
}

static bool isInt4PtxType(MMATypes type) {
  return (type == MMATypes::u4 || type == MMATypes::s4);
}

static bool isInt8PtxType(MMATypes type) {
  return (type == MMATypes::u8 || type == MMATypes::s8);
}

static bool isIntegerPtxType(MMATypes type) {
  return isInt4PtxType(type) || isInt8PtxType(type) || type == MMATypes::b1 ||
         type == MMATypes::s32;
}

MMATypes MmaOp::accumPtxType() {
  std::optional<mlir::NVVM::MMATypes> val = inferOperandMMAType(
      getODSOperands(2).getTypes().front(), /*isAccumulator=*/true);
  assert(val.has_value() && "accumulator PTX type should always be inferrable");
  return val.value();
}

MMATypes MmaOp::resultPtxType() {
  std::optional<mlir::NVVM::MMATypes> val =
      inferOperandMMAType(getResult().getType(), /*isAccumulator=*/true);
  assert(val.has_value() && "result PTX type should always be inferrable");
  return val.value();
}

void MmaOp::print(OpAsmPrinter &p) {
  SmallVector<Type, 4> regTypes;
  struct OperandFragment {
    StringRef operandName;
    StringRef ptxTypeAttr;
    SmallVector<Value, 4> regs;
    explicit OperandFragment(StringRef name, StringRef ptxTypeName)
        : operandName(name), ptxTypeAttr(ptxTypeName) {}
  };

  std::array<OperandFragment, 3> frags{
      OperandFragment("A", getMultiplicandAPtxTypeAttrName()),
      OperandFragment("B", getMultiplicandBPtxTypeAttrName()),
      OperandFragment("C", "")};
  SmallVector<StringRef, 4> ignoreAttrNames{
      mlir::NVVM::MmaOp::getOperandSegmentSizeAttr()};

  for (unsigned fragIdx = 0; fragIdx < frags.size(); fragIdx++) {
    auto &frag = frags[fragIdx];
    auto varOperandSpec = getODSOperandIndexAndLength(fragIdx);
    for (auto operandIdx = varOperandSpec.first;
         operandIdx < varOperandSpec.first + varOperandSpec.second;
         operandIdx++) {
      frag.regs.push_back(this->getOperand(operandIdx));
      if (operandIdx == 0) {
        regTypes.push_back(this->getOperand(operandIdx).getType());
      }
    }
    std::optional<MMATypes> inferredType =
        inferOperandMMAType(regTypes.back(), /*isAccumulator=*/fragIdx >= 2);
    if (inferredType)
      ignoreAttrNames.push_back(frag.ptxTypeAttr);
  }

  auto printMmaOperand = [&](const OperandFragment &frag) -> void {
    p << " " << frag.operandName;
    p << "[";
    p.printOperands(frag.regs);
    p << "] ";
  };

  for (const auto &frag : frags) {
    printMmaOperand(frag);
  }

  p.printOptionalAttrDict(this->getOperation()->getAttrs(), ignoreAttrNames);

  // Print the types of the operands and result.
  p << " : "
    << "(";
  llvm::interleaveComma(SmallVector<Type, 3>{frags[0].regs[0].getType(),
                                             frags[1].regs[0].getType(),
                                             frags[2].regs[0].getType()},
                        p);
  p << ")";
  p.printArrowTypeList(TypeRange{this->getRes().getType()});
}

void MmaOp::build(OpBuilder &builder, OperationState &result, Type resultType,
                  ValueRange operandA, ValueRange operandB, ValueRange operandC,
                  ArrayRef<int64_t> shape, std::optional<MMAB1Op> b1Op,
                  std::optional<MMAIntOverflow> intOverflow,
                  std::optional<std::array<MMATypes, 2>> multiplicandPtxTypes,
                  std::optional<std::array<MMALayout, 2>> multiplicandLayouts) {

  assert(shape.size() == 3 && "expected shape to have size 3 (m, n, k)");
  MLIRContext *ctx = builder.getContext();
  result.addAttribute(
      "shape", builder.getAttr<MMAShapeAttr>(shape[0], shape[1], shape[2]));

  result.addOperands(operandA);
  result.addOperands(operandB);
  result.addOperands(operandC);

  if (multiplicandPtxTypes) {
    result.addAttribute("multiplicandAPtxType",
                        MMATypesAttr::get(ctx, (*multiplicandPtxTypes)[0]));
    result.addAttribute("multiplicandBPtxType",
                        MMATypesAttr::get(ctx, (*multiplicandPtxTypes)[1]));
  } else {
    if (auto res = inferOperandMMAType(operandA[0].getType(), false))
      result.addAttribute("multiplicandAPtxType", MMATypesAttr::get(ctx, *res));
    if (auto res = inferOperandMMAType(operandB[0].getType(), false))
      result.addAttribute("multiplicandBPtxType", MMATypesAttr::get(ctx, *res));
  }

  if (multiplicandLayouts) {
    result.addAttribute("layoutA",
                        MMALayoutAttr::get(ctx, (*multiplicandLayouts)[0]));
    result.addAttribute("layoutB",
                        MMALayoutAttr::get(ctx, (*multiplicandLayouts)[1]));
  } else {
    result.addAttribute("layoutA", MMALayoutAttr::get(ctx, MMALayout::row));
    result.addAttribute("layoutB", MMALayoutAttr::get(ctx, MMALayout::col));
  }

  if (intOverflow.has_value())
    result.addAttribute("intOverflowBehavior",
                        MMAIntOverflowAttr::get(ctx, *intOverflow));
  if (b1Op.has_value())
    result.addAttribute("b1Op", MMAB1OpAttr::get(ctx, *b1Op));

  result.addTypes(resultType);
  result.addAttribute(
      MmaOp::getOperandSegmentSizeAttr(),
      builder.getDenseI32ArrayAttr({static_cast<int32_t>(operandA.size()),
                                    static_cast<int32_t>(operandB.size()),
                                    static_cast<int32_t>(operandC.size())}));
}

// <operation> :=
//   A `[` $operandA `]` B `[` $operandB `]` C `[` $operandC `]`
//   attr-dict : (type($operandA[0]), type($operandB[0]), type($operandC[0]))
//     `->` type($res)
ParseResult MmaOp::parse(OpAsmParser &parser, OperationState &result) {
  struct OperandFragment {
    std::optional<MMATypes> elemtype;
    SmallVector<OpAsmParser::UnresolvedOperand, 4> regs;
    SmallVector<Type> regTypes;
  };

  Builder &builder = parser.getBuilder();
  std::array<OperandFragment, 4> frags;

  NamedAttrList namedAttributes;

  // A helper to parse the operand segments.
  auto parseMmaOperand = [&](StringRef operandName,
                             OperandFragment &frag) -> LogicalResult {
    if (parser.parseKeyword(operandName).failed())
      return failure();
    if (parser
            .parseOperandList(frag.regs, OpAsmParser::Delimiter::OptionalSquare)
            .failed())
      return failure();
    return success();
  };

  // Parse the operand segments.
  if (parseMmaOperand("A", frags[0]).failed())
    return failure();
  if (parseMmaOperand("B", frags[1]).failed())
    return failure();
  if (parseMmaOperand("C", frags[2]).failed())
    return failure();

  if (parser.parseOptionalAttrDict(namedAttributes).failed())
    return failure();

  // Parse the type specification and resolve operands.
  SmallVector<Type, 3> operandTypes;
  if (failed(parser.parseColon()))
    return failure();
  if (failed(parser.parseLParen()))
    return failure();
  if (failed(parser.parseTypeList(operandTypes)))
    return failure();
  if (failed(parser.parseRParen()))
    if (operandTypes.size() != 3)
      return parser.emitError(
          parser.getNameLoc(),
          "expected one type for each operand segment but got " +
              Twine(operandTypes.size()) + " types");
  for (const auto &iter : llvm::enumerate(operandTypes)) {
    auto &frag = frags[iter.index()];
    frag.regTypes.resize(frag.regs.size(), iter.value());
    if (failed(parser.resolveOperands(frag.regs, frag.regTypes,
                                      parser.getNameLoc(), result.operands)))
      return failure();
    frag.elemtype = inferOperandMMAType(frag.regTypes[0],
                                        /*isAccumulator*/ iter.index() < 2);
  }

  Type resultType;
  if (parser.parseArrow() || parser.parseType(resultType))
    return failure();
  frags[3].elemtype = inferOperandMMAType(resultType, /*isAccumulator*/ true);

  std::array<StringRef, 2> names{"multiplicandAPtxType",
                                 "multiplicandBPtxType"};
  for (unsigned idx = 0; idx < names.size(); idx++) {
    const auto &frag = frags[idx];
    std::optional<NamedAttribute> attr = namedAttributes.getNamed(names[idx]);
    if (!frag.elemtype.has_value() && !attr.has_value()) {
      return parser.emitError(
          parser.getNameLoc(),
          "attribute " + names[idx] +
              " is not provided explicitly and cannot be inferred");
    }
    if (!attr.has_value())
      result.addAttribute(
          names[idx], MMATypesAttr::get(parser.getContext(), *frag.elemtype));
  }

  result.addTypes(resultType);
  if (!namedAttributes.empty())
    result.addAttributes(namedAttributes);
  result.addAttribute(MmaOp::getOperandSegmentSizeAttr(),
                      builder.getDenseI32ArrayAttr({
                          static_cast<int32_t>(frags[0].regs.size()),
                          static_cast<int32_t>(frags[1].regs.size()),
                          static_cast<int32_t>(frags[2].regs.size()),
                      }));
  return success();
}

LogicalResult MmaOp::verify() {
  MLIRContext *context = getContext();
  auto f16Ty = Float16Type::get(context);
  auto i32Ty = IntegerType::get(context, 32);
  auto f16x2Ty = VectorType::get(2, f16Ty);
  auto f32Ty = Float32Type::get(context);
  auto f16x2x4StructTy = LLVM::LLVMStructType::getLiteral(
      context, {f16x2Ty, f16x2Ty, f16x2Ty, f16x2Ty});

  auto s32x4StructTy =
      LLVM::LLVMStructType::getLiteral(context, {i32Ty, i32Ty, i32Ty, i32Ty});
  auto f32x8StructTy =
      LLVM::LLVMStructType::getLiteral(context, SmallVector<Type>(8, f32Ty));
  auto f16x2x2StructTy =
      LLVM::LLVMStructType::getLiteral(context, {f16x2Ty, f16x2Ty});
  auto f32x4StructTy =
      LLVM::LLVMStructType::getLiteral(context, {f32Ty, f32Ty, f32Ty, f32Ty});
  auto s32x2StructTy =
      LLVM::LLVMStructType::getLiteral(context, {i32Ty, i32Ty});

  std::array<int64_t, 3> mmaShape{getShapeAttr().getM(), getShapeAttr().getN(),
                                  getShapeAttr().getK()};

  // These variables define the set of allowed data types for matrices A, B, C,
  // and result.
  using AllowedShapes = SmallVector<std::array<int64_t, 3>, 2>;
  using AllowedTypes = SmallVector<SmallVector<Type, 4>, 2>;
  AllowedShapes allowedShapes;
  AllowedTypes expectedA;
  AllowedTypes expectedB;
  AllowedTypes expectedC;
  SmallVector<Type> expectedResult;

  // When M = 16, we just need to calculate the number of 8xk tiles, where
  // k is a factor that depends on the data type.
  if (mmaShape[0] == 16) {
    int64_t kFactor;
    Type multiplicandFragType;
    switch (*getMultiplicandAPtxType()) {
    case MMATypes::tf32:
      kFactor = 4;
      multiplicandFragType = i32Ty;
      expectedResult.push_back(LLVM::LLVMStructType::getLiteral(
          context, {f32Ty, f32Ty, f32Ty, f32Ty}));
      break;
    case MMATypes::bf16:
      kFactor = 8;
      multiplicandFragType = i32Ty;
      expectedResult.push_back(LLVM::LLVMStructType::getLiteral(
          context, {f32Ty, f32Ty, f32Ty, f32Ty}));
      break;
    case MMATypes::f16:
      kFactor = 8;
      multiplicandFragType = f16x2Ty;
      expectedResult.push_back(f16x2x2StructTy);
      expectedResult.push_back(f32x4StructTy);
      break;
    case MMATypes::s4:
    case MMATypes::u4:
      kFactor = 32;
      break;
    case MMATypes::b1:
      kFactor = 128;
      break;
    case MMATypes::s8:
    case MMATypes::u8:
      kFactor = 16;
      break;
    default:
      return emitError("invalid shape or multiplicand type: " +
                       stringifyEnum(getMultiplicandAPtxType().value()));
    }

    if (isIntegerPtxType(getMultiplicandAPtxType().value())) {
      expectedResult.push_back(s32x4StructTy);
      expectedC.emplace_back(4, i32Ty);
      multiplicandFragType = i32Ty;
    } else {
      expectedC.emplace_back(2, f16x2Ty);
      expectedC.emplace_back(4, f32Ty);
    }

    int64_t unitA = (mmaShape[0] / 8) * (mmaShape[2] / kFactor);
    int64_t unitB = (mmaShape[1] / 8) * (mmaShape[2] / kFactor);
    expectedA.emplace_back(unitA, multiplicandFragType);
    expectedB.emplace_back(unitB, multiplicandFragType);
    allowedShapes.push_back({16, 8, kFactor});
    allowedShapes.push_back({16, 8, kFactor * 2});
  }

  // In the M=8 case, there is only 1 possible case per data type.
  if (mmaShape[0] == 8) {
    if (*getMultiplicandAPtxType() == MMATypes::f16) {
      expectedA.emplace_back(2, f16x2Ty);
      expectedB.emplace_back(2, f16x2Ty);
      expectedResult.push_back(f16x2x4StructTy);
      expectedResult.push_back(f32x8StructTy);
      expectedC.emplace_back(4, f16x2Ty);
      expectedC.emplace_back(8, f32Ty);
      allowedShapes.push_back({8, 8, 4});
    }
    if (*getMultiplicandAPtxType() == MMATypes::f64) {
      Type f64Ty = Float64Type::get(context);
      expectedA.emplace_back(1, f64Ty);
      expectedB.emplace_back(1, f64Ty);
      expectedC.emplace_back(2, f64Ty);
      expectedResult.emplace_back(LLVM::LLVMStructType::getLiteral(
          context, SmallVector<Type>(2, f64Ty)));
      allowedShapes.push_back({8, 8, 4});
    }
    if (isIntegerPtxType(getMultiplicandAPtxType().value())) {
      expectedA.push_back({i32Ty});
      expectedB.push_back({i32Ty});
      expectedC.push_back({i32Ty, i32Ty});
      expectedResult.push_back(s32x2StructTy);
      if (isInt4PtxType(getMultiplicandAPtxType().value()))
        allowedShapes.push_back({8, 8, 32});
      if (isInt8PtxType(getMultiplicandAPtxType().value()))
        allowedShapes.push_back({8, 8, 16});
      if (getMultiplicandAPtxType().value() == MMATypes::b1)
        allowedShapes.push_back({8, 8, 128});
    }
  }

  std::string errorMessage;
  llvm::raw_string_ostream errorStream(errorMessage);

  // Check that we matched an existing shape/dtype combination.
  if (expectedA.empty() || expectedB.empty() || expectedC.empty() ||
      !llvm::is_contained(allowedShapes, mmaShape)) {
    errorStream << "unimplemented variant for MMA shape <";
    llvm::interleaveComma(mmaShape, errorStream);
    errorStream << ">";
    return emitOpError(errorMessage);
  }

  // Verify the operand types for segments of A, B, and C operands.
  std::array<StringRef, 3> operandNames{"A", "B", "C"};
  for (const auto &iter : llvm::enumerate(
           SmallVector<AllowedTypes, 3>{expectedA, expectedB, expectedC})) {
    auto spec = this->getODSOperandIndexAndLength(iter.index());
    SmallVector<Type, 4> operandTySeg(operand_type_begin() + spec.first,
                                      operand_type_begin() + spec.first +
                                          spec.second);
    bool match = llvm::is_contained(iter.value(), operandTySeg);

    if (!match) {
      errorStream << "Could not match types for the "
                  << operandNames[iter.index()]
                  << " operands; expected one of ";
      for (const auto &x : iter.value()) {
        errorStream << x.size() << "x" << x[0] << " ";
      }
      errorStream << "but got ";
      llvm::interleaveComma(operandTySeg, errorStream);
      return emitOpError(errorMessage);
    }
  }

  // Check the result type
  if (!llvm::any_of(expectedResult, [&](Type expectedResultType) {
        return expectedResultType == getResult().getType();
      })) {
    errorStream
        << "Could not match allowed types for the result; expected one of ";
    llvm::interleaveComma(expectedResult, errorStream);
    errorStream << " but got " << getResult().getType();
    return emitOpError(errorMessage);
  }

  // Ensure that binary MMA variants have a b1 MMA operation defined.
  if (getMultiplicandAPtxType() == MMATypes::b1 && !getB1Op()) {
    return emitOpError("op requires " + getB1OpAttrName().strref() +
                       " attribute");
  }

  // Ensure int4/int8 MMA variants specify the accum overflow behavior
  // attribute.
  if (isInt4PtxType(*getMultiplicandAPtxType()) ||
      isInt8PtxType(*getMultiplicandAPtxType())) {
    if (!getIntOverflowBehavior())
      return emitOpError("op requires " +
                         getIntOverflowBehaviorAttrName().strref() +
                         " attribute");
  }

  return success();
}

LogicalResult ShflOp::verify() {
  if (!(*this)->getAttrOfType<UnitAttr>("return_value_and_is_valid"))
    return success();
  auto type = llvm::dyn_cast<LLVM::LLVMStructType>(getType());
  auto elementType = (type && type.getBody().size() == 2)
                         ? llvm::dyn_cast<IntegerType>(type.getBody()[1])
                         : nullptr;
  if (!elementType || elementType.getWidth() != 1)
    return emitError("expected return type to be a two-element struct with "
                     "i1 as the second element");
  return success();
}

std::pair<mlir::Type, unsigned> NVVM::inferMMAType(NVVM::MMATypes type,
                                                   NVVM::MMAFrag frag, int nRow,
                                                   int nCol,
                                                   MLIRContext *context) {
  unsigned numberElements = 0;
  Type elementType;
  OpBuilder builder(context);
  Type f16x2 = VectorType::get(2, builder.getF16Type());
  if (type == NVVM::MMATypes::f16) {
    elementType = f16x2;
    if (frag == NVVM::MMAFrag::a || frag == NVVM::MMAFrag::b)
      numberElements = 8;
    else
      numberElements = 4;
  } else if (type == NVVM::MMATypes::f32) {
    elementType = builder.getF32Type();
    numberElements = 8;
  } else if (type == NVVM::MMATypes::tf32) {
    elementType = builder.getI32Type();
    numberElements = 4;
  } else if (type == NVVM::MMATypes::s8 || type == NVVM::MMATypes::u8) {
    elementType = builder.getI32Type();
    int parallelSize = 0;
    if (frag == NVVM::MMAFrag::a)
      parallelSize = nRow;
    if (frag == NVVM::MMAFrag::b)
      parallelSize = nCol;

    // m == 16 && n == 16 && k == 16
    if (parallelSize == 16)
      numberElements = 2;
    // m == 8 && n == 32 && k == 16 or m == 32 && n == 8 && k == 16
    else if (parallelSize == 8)
      numberElements = 1;
    else if (parallelSize == 32)
      numberElements = 4;
  } else if (type == NVVM::MMATypes::s32) {
    elementType = builder.getI32Type();
    numberElements = 8;
  }
  assert(numberElements != 0 && elementType != nullptr);
  return std::make_pair(elementType, numberElements);
}

static std::pair<mlir::Type, unsigned>
inferMMATypeFromMNK(NVVM::MMATypes type, NVVM::MMAFrag frag, int m, int n,
                    int k, MLIRContext *context) {
  int nRow, nCol;
  if (frag == NVVM::MMAFrag::a) {
    nRow = m;
    nCol = k;
  } else if (frag == NVVM::MMAFrag::b) {
    nRow = k;
    nCol = n;
  } else {
    nRow = m;
    nCol = n;
  }
  assert(nRow && nCol);
  return inferMMAType(type, frag, nRow, nCol, context);
}

LogicalResult NVVM::WMMALoadOp::verify() {
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getPtr().getType()).getAddressSpace();
  if (addressSpace != 0 && addressSpace != NVVM::kGlobalMemorySpace &&
      addressSpace != NVVM::kSharedMemorySpace)
    return emitOpError("expected source pointer in memory "
                       "space 0, 1, 3");

  if (NVVM::WMMALoadOp::getIntrinsicID(getM(), getN(), getK(), getLayout(),
                                       getEltype(), getFrag()) == 0)
    return emitOpError() << "invalid attribute combination";
  std::pair<Type, unsigned> typeInfo = inferMMATypeFromMNK(
      getEltype(), getFrag(), getM(), getN(), getK(), getContext());
  Type dstType = LLVM::LLVMStructType::getLiteral(
      getContext(), SmallVector<Type, 8>(typeInfo.second, typeInfo.first));
  if (getType() != dstType)
    return emitOpError("expected destination type is a structure of ")
           << typeInfo.second << " elements of type " << typeInfo.first;
  return success();
}

LogicalResult NVVM::WMMAStoreOp::verify() {
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getPtr().getType()).getAddressSpace();
  if (addressSpace != 0 && addressSpace != NVVM::kGlobalMemorySpace &&
      addressSpace != NVVM::kSharedMemorySpace)
    return emitOpError("expected operands to be a source pointer in memory "
                       "space 0, 1, 3");

  if (NVVM::WMMAStoreOp::getIntrinsicID(getM(), getN(), getK(), getLayout(),
                                        getEltype()) == 0)
    return emitOpError() << "invalid attribute combination";
  std::pair<Type, unsigned> typeInfo = inferMMATypeFromMNK(
      getEltype(), NVVM::MMAFrag::c, getM(), getN(), getK(), getContext());
  if (getArgs().size() != typeInfo.second)
    return emitOpError() << "expected " << typeInfo.second << " data operands";
  if (llvm::any_of(getArgs(), [&typeInfo](Value operands) {
        return operands.getType() != typeInfo.first;
      }))
    return emitOpError() << "expected data operands of type " << typeInfo.first;
  return success();
}

LogicalResult NVVM::WMMAMmaOp::verify() {
  if (NVVM::WMMAMmaOp::getIntrinsicID(getM(), getN(), getK(), getLayoutA(),
                                      getLayoutB(), getEltypeA(),
                                      getEltypeB()) == 0)
    return emitOpError() << "invalid attribute combination";
  std::pair<Type, unsigned> typeInfoA = inferMMATypeFromMNK(
      getEltypeA(), NVVM::MMAFrag::a, getM(), getN(), getK(), getContext());
  std::pair<Type, unsigned> typeInfoB = inferMMATypeFromMNK(
      getEltypeA(), NVVM::MMAFrag::b, getM(), getN(), getK(), getContext());
  std::pair<Type, unsigned> typeInfoC = inferMMATypeFromMNK(
      getEltypeB(), NVVM::MMAFrag::c, getM(), getN(), getK(), getContext());
  SmallVector<Type, 32> arguments;
  arguments.append(typeInfoA.second, typeInfoA.first);
  arguments.append(typeInfoB.second, typeInfoB.first);
  arguments.append(typeInfoC.second, typeInfoC.first);
  unsigned numArgs = arguments.size();
  if (getArgs().size() != numArgs)
    return emitOpError() << "expected " << numArgs << " arguments";
  for (unsigned i = 0; i < numArgs; i++) {
    if (getArgs()[i].getType() != arguments[i])
      return emitOpError() << "expected argument " << i << " to be of type "
                           << arguments[i];
  }
  Type dstType = LLVM::LLVMStructType::getLiteral(
      getContext(), SmallVector<Type, 8>(typeInfoC.second, typeInfoC.first));
  if (getType() != dstType)
    return emitOpError("expected destination type is a structure of ")
           << typeInfoC.second << " elements of type " << typeInfoC.first;
  return success();
}

LogicalResult NVVM::LdMatrixOp::verify() {
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getPtr().getType()).getAddressSpace();
  if (addressSpace != NVVM::kSharedMemorySpace)
    return emitOpError("expected source pointer in memory space 3");

  if (getNum() != 1 && getNum() != 2 && getNum() != 4)
    return emitOpError("expected num attribute to be 1, 2 or 4");

  Type i32 = IntegerType::get(getContext(), 32);
  if (getNum() == 1 && getType() != i32)
    return emitOpError("expected destination type is i32");
  if (getNum() == 2 || getNum() == 4) {
    Type dstType = LLVM::LLVMStructType::getLiteral(
        getContext(), SmallVector<Type>(getNum(), i32));
    if (getType() != dstType)
      return emitOpError("expected destination type is a structure of ")
             << getNum() << " elements of type i32";
  }
  return success();
}

LogicalResult NVVM::StMatrixOp::verify() {
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getPtr().getType()).getAddressSpace();
  if (addressSpace != NVVM::kSharedMemorySpace)
    return emitOpError("expected source pointer in memory space 3");

  int numMatrix = getSources().size();
  if (numMatrix != 1 && numMatrix != 2 && numMatrix != 4)
    return emitOpError("expected num attribute to be 1, 2 or 4");

  return success();
}

FailureOr<int> getAllowedSizeK(NVVM::WGMMATypes typeA) {
  if (typeA == NVVM::WGMMATypes::tf32)
    return 8;
  if (typeA == NVVM::WGMMATypes::f16 || typeA == NVVM::WGMMATypes::bf16)
    return 16;
  if (typeA == NVVM::WGMMATypes::s8 || typeA == NVVM::WGMMATypes::u8)
    return 32;
  if (typeA == NVVM::WGMMATypes::e4m3 || typeA == NVVM::WGMMATypes::e5m2)
    return 32;
  if (typeA == NVVM::WGMMATypes::b1)
    return 256;
  return failure();
}

LogicalResult isAllowedWGMMADataType(NVVM::WGMMATypes typeD,
                                     NVVM::WGMMATypes typeA,
                                     NVVM::WGMMATypes typeB) {
  switch (typeA) {
  case NVVM::WGMMATypes::f16:
    if ((typeD == NVVM::WGMMATypes::f32 || typeD == NVVM::WGMMATypes::f16) &&
        typeB == NVVM::WGMMATypes::f16)
      return success();
    break;
  case NVVM::WGMMATypes::tf32:
    if (typeD == NVVM::WGMMATypes::f32 && typeB == NVVM::WGMMATypes::tf32)
      return success();
    break;
  case NVVM::WGMMATypes::u8:
  case NVVM::WGMMATypes::s8:
    if (typeD == NVVM::WGMMATypes::s32 &&
        (typeB == NVVM::WGMMATypes::u8 || typeB == NVVM::WGMMATypes::s8))
      return success();
    break;
  case NVVM::WGMMATypes::b1:
    if (typeD == NVVM::WGMMATypes::s32 && typeB == NVVM::WGMMATypes::b1)
      return success();
    break;
  case NVVM::WGMMATypes::bf16:
    if ((typeD == NVVM::WGMMATypes::f32 || typeD == NVVM::WGMMATypes::f16) &&
        typeB == NVVM::WGMMATypes::bf16)
      return success();
    break;
  case NVVM::WGMMATypes::e4m3:
  case NVVM::WGMMATypes::e5m2:
    if ((typeD == NVVM::WGMMATypes::f32 || typeD == NVVM::WGMMATypes::f16) &&
        (typeB == NVVM::WGMMATypes::e5m2 || typeB == NVVM::WGMMATypes::e4m3))
      return success();
    break;
  case WGMMATypes::f32:
  case WGMMATypes::s32:
    llvm_unreachable("unsupported input types");
    break;
  }
  return failure();
}

LogicalResult isAllowedSizeN(int sizeN, NVVM::WGMMATypes typeA) {
  SmallVector<int> allowedN = {8,   16,  24,  32,  40,  48,  56,  64,
                               72,  80,  88,  96,  104, 112, 120, 128,
                               136, 144, 152, 160, 168, 176, 184, 192,
                               200, 208, 216, 224, 232, 240, 248, 256};
  SmallVector<int> allowedNshort = {8,   16,  24,  32,  48,  64,
                                    80,  96,  112, 128, 144, 160,
                                    176, 192, 208, 224, 240, 256};
  switch (typeA) {
  case WGMMATypes::f16:
  case WGMMATypes::tf32:
  case WGMMATypes::bf16:
  case WGMMATypes::e4m3:
  case WGMMATypes::e5m2:
    if (llvm::is_contained(allowedN, sizeN))
      return success();
    break;
  case WGMMATypes::u8:
  case WGMMATypes::s8:
  case WGMMATypes::b1:
    if (llvm::is_contained(allowedNshort, sizeN))
      return success();
    break;
  case WGMMATypes::f32:
  case WGMMATypes::s32:
    llvm_unreachable("unsupported input types");
    break;
  }
  return failure();
}

LogicalResult NVVM::WgmmaMmaAsyncOp::verify() {
  Value outValue = getResults();
  auto stype = dyn_cast<LLVM::LLVMStructType>(outValue.getType());
  if (!stype)
    return emitOpError() << "expected results to be struct";
  int outputSize = stype.getBody().size();
  WGMMATypes typeD = getTypeD();
  WGMMATypes typeA = getTypeA();
  WGMMATypes typeB = getTypeB();

  for (Type t : stype.getBody()) {
    if (t != stype.getBody().front())
      return emitOpError()
             << "all elements in struct must be same type but there is " << t;
  }

  if (typeD != WGMMATypes::f32 && typeD != WGMMATypes::f16 &&
      typeD != WGMMATypes::s32) {
    return emitOpError() << "does not support the given output type "
                         << NVVM::stringifyWGMMATypes(typeD);
  }
  if (typeD == WGMMATypes::s32 &&
      (getScaleA() == WGMMAScaleIn::neg || getScaleB() == WGMMAScaleIn::neg)) {
    return emitOpError() << "has s32 output, scaleA and scaleB cannot be neg";
  }

  if (failed(isAllowedWGMMADataType(typeD, typeA, typeB))) {
    return emitOpError() << NVVM::stringifyWGMMATypes(typeD)
                         << " += " << NVVM::stringifyWGMMATypes(typeA) << " * "
                         << NVVM::stringifyWGMMATypes(typeB)
                         << ", it is not supported.";
  }

  // Check M
  if (getShape().getM() != 64)
    return emitOpError() << "shape 'm' must be 64";

  // Check K
  FailureOr<int> allowedK = getAllowedSizeK(typeA);
  if (failed(allowedK) || allowedK.value() != getShape().getK())
    return emitOpError() << "shape 'k' must be " << allowedK.value()
                         << " for input type "
                         << NVVM::stringifyWGMMATypes(typeA);

  // Check N
  if (failed(isAllowedSizeN(getShape().getN(), typeA))) {
    return emitOpError() << "has input type "
                         << NVVM::stringifyWGMMATypes(typeA) << " n is set to "
                         << getShape().getN() << ", it is not supported.";
  }

  // Check transpose (only available for f16/bf16)
  // Matrices A should be stored in row-major and B in column-major.
  // Only f16/bf16 matrices can be stored in either column-major or row-major
  // by setting the transpose value(imm-trans-a,imm-trans-b) in PTX code.
  if ((typeA != WGMMATypes::f16 && typeA != WGMMATypes::bf16) &&
      (getLayoutA() == mlir::NVVM::MMALayout::col ||
       getLayoutB() == mlir::NVVM::MMALayout::row)) {
    return emitOpError()
           << "given layouts layout_a = " << stringifyMMALayout(getLayoutA())
           << " and layout_b = " << stringifyMMALayout(getLayoutB())
           << " for input types " << stringifyWGMMATypes(typeA) << " and "
           << stringifyWGMMATypes(typeB)
           << " requires transpose. However, this is only supported for: "
           << stringifyMMATypes(MMATypes::f16) << " and "
           << stringifyMMATypes(MMATypes::bf16);
  }

  // Check result registers
  int expectedOutput = 0;
  if (typeD == WGMMATypes::f32 || typeD == WGMMATypes::s32)
    expectedOutput = getShape().getN() / 2;
  if (typeD == WGMMATypes::f16)
    expectedOutput = getShape().getN() / 4;
  if (outputSize != expectedOutput) {
    return emitOpError() << "results " << expectedOutput
                         << ", however output struct has " << outputSize
                         << " elements";
  }
  // Check satfinite (only available for s32 accumulator)
  if (typeD != WGMMATypes::s32 &&
      getSatfinite().value_or(NVVM::MMAIntOverflow::wrapped) ==
          NVVM::MMAIntOverflow::satfinite) {
    return emitOpError()
           << " `satfinite` can be only used with s32 accumulator, however "
              "the current accumulator is "
           << NVVM::stringifyWGMMATypes(typeD);
  }

  return success();
}

std::string NVVM::WgmmaMmaAsyncOp::getPtx() {

  int m = getShape().getM(), n = getShape().getN(), k = getShape().getK();
  bool isF16 = getTypeA() == WGMMATypes::f16 || getTypeA() == WGMMATypes::bf16;

  StringRef outputTypeName = stringifyWGMMATypes(getTypeD());

  int expectedOutputRegisters = 0;
  if (getTypeD() == WGMMATypes::f16)
    expectedOutputRegisters = getShape().getN() / 4;
  else
    expectedOutputRegisters = getShape().getN() / 2;

  std::string ptx;
  llvm::raw_string_ostream ss(ptx);

  ss << "{\n"
        ".reg .pred p;\n"
        "setp.ne.b32 p, $"
     << ((expectedOutputRegisters * 2) + 2)
     << ", 0;\n"
        "wgmma.mma_async.sync.aligned.m"
     << m << "n" << n << "k" << k << "." << outputTypeName << "."
     << stringifyWGMMATypes(getTypeA()) << "."
     << stringifyWGMMATypes(getTypeB());
  if (getSatfinite().value_or(NVVM::MMAIntOverflow::wrapped) ==
      NVVM::MMAIntOverflow::satfinite)
    ss << ".satfinite";
  ss << " {";
  int regCnt = 0;
  for (; regCnt < expectedOutputRegisters; ++regCnt) {
    ss << "$" << regCnt;
    if (regCnt != expectedOutputRegisters - 1)
      ss << ", ";
  }

  ss << "},";
  // Need to map read/write registers correctly.
  regCnt = (regCnt * 2);
  ss << " $" << (regCnt) << ","
     << " $" << (regCnt + 1) << ","
     << " p";
  if (getTypeD() != WGMMATypes::s32) {
    ss << ", $" << (regCnt + 3) << ",  $" << (regCnt + 4);
  }
  // Don't add transpose parameters unless needed.
  if (isF16) {
    ss << ", $" << (regCnt + 5) << ",  $" << (regCnt + 6);
  }
  ss << ";\n"
     << "}\n";
  return ptx;
}

void NVVM::WgmmaMmaAsyncOp::getAsmValues(
    RewriterBase &rewriter,
    llvm::SmallVectorImpl<std::pair<mlir::Value, mlir::NVVM::PTXRegisterMod>>
        &asmValues) {
  bool isF16 = getTypeA() == WGMMATypes::f16 || getTypeA() == WGMMATypes::bf16;
  if (getResults())
    asmValues.push_back({getResults(), mlir::NVVM::PTXRegisterMod::Write});
  if (getInouts())
    asmValues.push_back({getInouts(), mlir::NVVM::PTXRegisterMod::ReadWrite});
  asmValues.push_back({getDescriptorA(), mlir::NVVM::PTXRegisterMod::Read});
  asmValues.push_back({getDescriptorB(), mlir::NVVM::PTXRegisterMod::Read});
  asmValues.push_back({makeConstantI32(rewriter, static_cast<int>(getScaleD())),
                       mlir::NVVM::PTXRegisterMod::Read});
  if (getTypeD() != WGMMATypes::s32) {
    asmValues.push_back(
        {makeConstantI32(rewriter,
                         getScaleA() == NVVM::WGMMAScaleIn::neg ? -1 : 1),
         mlir::NVVM::PTXRegisterMod::Read});
    asmValues.push_back(
        {makeConstantI32(rewriter,
                         getScaleB() == NVVM::WGMMAScaleIn::neg ? -1 : 1),
         mlir::NVVM::PTXRegisterMod::Read});
  }
  if (isF16) {
    asmValues.push_back(
        {makeConstantI32(rewriter, static_cast<int>(getLayoutA())),
         mlir::NVVM::PTXRegisterMod::Read});
    asmValues.push_back(
        {makeConstantI32(rewriter, 1 - static_cast<int>(getLayoutB())),
         mlir::NVVM::PTXRegisterMod::Read});
  }
}
LogicalResult NVVM::FenceProxyOp::verify() {
  if (getKind() == NVVM::ProxyKind::TENSORMAP)
    return emitOpError() << "tensormap proxy is not a supported proxy kind";
  if (getKind() == NVVM::ProxyKind::GENERIC)
    return emitOpError() << "generic proxy not a supported proxy kind";
  if (getKind() == NVVM::ProxyKind::async_shared && !getSpace().has_value()) {
    return emitOpError() << "async_shared fence requires space attribute";
  }
  if (getKind() != NVVM::ProxyKind::async_shared && getSpace().has_value()) {
    return emitOpError() << "only async_shared fence can have space attribute";
  }
  return success();
}

LogicalResult NVVM::FenceProxyAcquireOp::verify() {
  if (getFromProxy() != NVVM::ProxyKind::GENERIC)
    return emitOpError("uni-directional proxies only support generic for "
                       "from_proxy attribute");

  if (getToProxy() != NVVM::ProxyKind::TENSORMAP)
    return emitOpError("uni-directional proxies only support tensormap "
                       "for to_proxy attribute");

  return success();
}

LogicalResult NVVM::FenceProxyReleaseOp::verify() {
  if (getFromProxy() != NVVM::ProxyKind::GENERIC)
    return emitOpError("uni-directional proxies only support generic for "
                       "from_proxy attribute");

  if (getToProxy() != NVVM::ProxyKind::TENSORMAP)
    return emitOpError("uni-directional proxies only support tensormap "
                       "for to_proxy attribute");

  return success();
}

LogicalResult NVVM::SetMaxRegisterOp::verify() {
  if (getRegCount() % 8)
    return emitOpError("new register size must be multiple of 8");
  if (getRegCount() < 24 || getRegCount() > 256)
    return emitOpError("new register size must be in between 24 to 256");
  return success();
}

LogicalResult NVVM::BarrierOp::verify() {
  if (getNumberOfThreads() && !getBarrierId())
    return emitOpError(
        "barrier id is missing, it should be set between 0 to 15");
  return success();
}

LogicalResult NVVM::Tcgen05CpOp::verify() {
  auto mc = getMulticast();

  using SH = Tcgen05CpShape;
  using MC = Tcgen05CpMulticast;
  switch (getShape()) {
  case SH::SHAPE_128x256b:
  case SH::SHAPE_128x128b:
  case SH::SHAPE_4x256b:
    if (mc != MC::NONE)
      return emitError("Invalid multicast type for tcgen05.cp Op");
    break;
  case SH::SHAPE_64x128b:
    if (mc != MC::WARPX2_01_23 && mc != MC::WARPX2_02_13)
      return emitError("Shape 64x128b requires multicast warpx2_01_23 or "
                       "warpx2_02_13 for tcgen05.cp Op");
    break;
  case SH::SHAPE_32x128b:
    if (mc != MC::WARPX4)
      return emitError(
          "Shape 32x128b requires multicast warpx4 for tcgen05.cp Op");
    break;
  }
  return success();
}

LogicalResult NVVM::MatchSyncOp::verify() {
  if (getKind() == NVVM::MatchSyncKind::all) {
    auto type = llvm::dyn_cast<LLVM::LLVMStructType>(getType());
    if (!type || type.getBody().size() != 2 ||
        !type.getBody()[0].isInteger(32) || !type.getBody()[1].isInteger(1)) {
      return emitOpError("match.sync 'all' returns a two element struct with "
                         "first element as i32 and second element as i1");
    }
  } else {
    if (!getType().isInteger(32)) {
      return emitOpError("match.sync 'any' returns an i32");
    }
  }
  return success();
}

LogicalResult NVVM::VoteSyncOp::verify() {
  if (getKind() == NVVM::VoteSyncKind::ballot) {
    if (!getType().isInteger(32)) {
      return emitOpError("vote.sync 'ballot' returns an i32");
    }
  } else {
    if (!getType().isInteger(1)) {
      return emitOpError("vote.sync 'any', 'all' and 'uni' returns an i1");
    }
  }
  return success();
}

LogicalResult NVVM::PrefetchOp::verify() {
  using MemSpace = NVVM::NVVMMemorySpace;
  using CacheLevel = NVVM::PrefetchCacheLevel;

  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(getAddr().getType()).getAddressSpace();
  std::optional<NVVM::CacheEvictionPriority> evictPriority = getEvictPriority();

  if (getUniform()) {
    if (getCacheLevel() != CacheLevel::L1)
      return emitOpError("unsupported cache level, the only supported uniform "
                         "cache level is L1");

    if (addressSpace != MemSpace::kGenericMemorySpace)
      return emitOpError(
          "prefetch to uniform cache requires a generic pointer");
  }

  if (evictPriority) {
    if (getCacheLevel() != CacheLevel::L2)
      return emitOpError(
          "cache eviction priority supported only for cache level L2");

    if (addressSpace != MemSpace::kGlobalMemorySpace)
      return emitOpError("cache eviction priority requires a global pointer");

    if (*evictPriority != NVVM::CacheEvictionPriority::EvictNormal &&
        *evictPriority != NVVM::CacheEvictionPriority::EvictLast)
      return emitOpError(
          "unsupported cache eviction priority, only evict_last and "
          "evict_normal are supported");
  }

  return success();
}

/// Packs the given `field` into the `result`.
/// The `result` is 64-bits and each `field` can be 32-bits or narrower.
static llvm::Value *
packValInto64Bits(llvm::IRBuilderBase &builder,
                  llvm::Value *result, // the `result` (unset bits are zero)
                  llvm::Value *field,  // `field` to pack into `result`
                  unsigned sizeInBits, // Size of `field` in bits
                  unsigned start) {    // Starting bit within `result`
  field = builder.CreateZExtOrBitCast(field, builder.getInt32Ty());

  unsigned mask = (sizeInBits < 32 ? ((1u << sizeInBits) - 1) : 0xffffffffu);
  if (mask != 0xffffffffu)
    field = builder.CreateAnd(field, builder.getInt32(mask));

  field = builder.CreateZExtOrBitCast(field, builder.getInt64Ty());
  field = builder.CreateShl(field, start);

  return builder.CreateOr(result, field);
}

void Tcgen05MmaSmemDescOp::createSmemDescriptor(Operation &op,
                                                LLVM::ModuleTranslation &mt,
                                                llvm::IRBuilderBase &builder) {
  auto thisOp = cast<NVVM::Tcgen05MmaSmemDescOp>(op);
  llvm::Value *smemDesc = builder.getInt64(0);

  smemDesc = packValInto64Bits(builder, smemDesc,
                               mt.lookupValue(thisOp.getStartAddr()), 14, 0);
  smemDesc = packValInto64Bits(
      builder, smemDesc, mt.lookupValue(thisOp.getLeadingDimOffset()), 14, 16);
  smemDesc = packValInto64Bits(
      builder, smemDesc, mt.lookupValue(thisOp.getStrideDimOffset()), 14, 32);

  smemDesc = packValInto64Bits(builder, smemDesc, builder.getInt32(1), 3, 46);
  smemDesc = packValInto64Bits(builder, smemDesc,
                               mt.lookupValue(thisOp.getBaseOffset()), 3, 49);
  smemDesc = packValInto64Bits(
      builder, smemDesc, mt.lookupValue(thisOp.getLeadingDimMode()), 1, 52);
  smemDesc = packValInto64Bits(builder, smemDesc,
                               mt.lookupValue(thisOp.getSwizzleMode()), 3, 61);

  mt.mapValue(thisOp.getRes()) = smemDesc;
}

//===----------------------------------------------------------------------===//
// getIntrinsicID/getIntrinsicIDAndArgs methods
//===----------------------------------------------------------------------===//

#define CP_ASYNC_ID_IMPL(mod, size, suffix)                                    \
  llvm::Intrinsic::nvvm_cp_async_##mod##_shared_global_##size##suffix

#define GET_CP_ASYNC_ID(mod, size, has_cpsize)                                 \
  has_cpsize ? CP_ASYNC_ID_IMPL(mod, size, _s) : CP_ASYNC_ID_IMPL(mod, size, )

llvm::Intrinsic::ID
CpAsyncOp::getIntrinsicIDAndArgs(Operation &op, LLVM::ModuleTranslation &mt,
                                 llvm::SmallVector<llvm::Value *> &args) {
  llvm::Intrinsic::ID id;

  auto cpAsyncOp = cast<NVVM::CpAsyncOp>(op);
  bool hasCpSize = static_cast<bool>(cpAsyncOp.getCpSize());
  switch (cpAsyncOp.getSize()) {
  case 4:
    id = GET_CP_ASYNC_ID(ca, 4, hasCpSize);
    break;
  case 8:
    id = GET_CP_ASYNC_ID(ca, 8, hasCpSize);
    break;
  case 16:
    id = (cpAsyncOp.getModifier() == NVVM::LoadCacheModifierKind::CG)
             ? GET_CP_ASYNC_ID(cg, 16, hasCpSize)
             : GET_CP_ASYNC_ID(ca, 16, hasCpSize);
    break;
  default:
    llvm_unreachable("Invalid copy size in CpAsyncOp.");
  }

  // Fill the Intrinsic Args
  args.push_back(mt.lookupValue(cpAsyncOp.getDst()));
  args.push_back(mt.lookupValue(cpAsyncOp.getSrc()));
  if (hasCpSize)
    args.push_back(mt.lookupValue(cpAsyncOp.getCpSize()));

  return id;
}

mlir::NVVM::IDArgPair CpAsyncBulkPrefetchOp::getIntrinsicIDAndArgs(
    Operation &op, LLVM::ModuleTranslation &mt, llvm::IRBuilderBase &builder) {
  auto thisOp = cast<NVVM::CpAsyncBulkPrefetchOp>(op);
  llvm::SmallVector<llvm::Value *> args;
  llvm::Intrinsic::ID id = llvm::Intrinsic::nvvm_cp_async_bulk_prefetch_L2;

  // Fill the Intrinsic Args
  args.push_back(mt.lookupValue(thisOp.getSrcMem()));
  args.push_back(mt.lookupValue(thisOp.getSize()));

  mlir::Value cacheHint = thisOp.getL2CacheHint();
  const bool hasCacheHint = static_cast<bool>(cacheHint);
  llvm::Value *i64Unused =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(mt.getLLVMContext()), 0);
  args.push_back(hasCacheHint ? mt.lookupValue(cacheHint) : i64Unused);
  args.push_back(builder.getInt1(hasCacheHint));

  return {id, std::move(args)};
}

mlir::NVVM::IDArgPair CpAsyncBulkSharedCTAToGlobalOp::getIntrinsicIDAndArgs(
    Operation &op, LLVM::ModuleTranslation &mt, llvm::IRBuilderBase &builder) {
  auto thisOp = cast<NVVM::CpAsyncBulkSharedCTAToGlobalOp>(op);
  llvm::SmallVector<llvm::Value *> args;
  llvm::Intrinsic::ID id =
      llvm::Intrinsic::nvvm_cp_async_bulk_shared_cta_to_global;

  // Fill the Intrinsic Args
  args.push_back(mt.lookupValue(thisOp.getDstMem()));
  args.push_back(mt.lookupValue(thisOp.getSrcMem()));
  args.push_back(mt.lookupValue(thisOp.getSize()));

  mlir::Value cacheHint = thisOp.getL2CacheHint();
  const bool hasCacheHint = static_cast<bool>(cacheHint);
  llvm::Value *i64Unused =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(mt.getLLVMContext()), 0);
  args.push_back(hasCacheHint ? mt.lookupValue(cacheHint) : i64Unused);
  args.push_back(builder.getInt1(hasCacheHint));

  // Choose the bytemask variant
  if (mlir::Value byteMask = thisOp.getByteMask()) {
    args.push_back(mt.lookupValue(byteMask));
    id = llvm::Intrinsic::nvvm_cp_async_bulk_shared_cta_to_global_bytemask;
  }

  return {id, std::move(args)};
}

llvm::Intrinsic::ID CpAsyncBulkTensorPrefetchOp::getIntrinsicID(int tensorDims,
                                                                bool isIm2Col) {
  switch (tensorDims) {
  case 1:
    return llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_tile_1d;
  case 2:
    return llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_tile_2d;
  case 3:
    return isIm2Col
               ? llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_im2col_3d
               : llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_tile_3d;
  case 4:
    return isIm2Col
               ? llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_im2col_4d
               : llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_tile_4d;
  case 5:
    return isIm2Col
               ? llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_im2col_5d
               : llvm::Intrinsic::nvvm_cp_async_bulk_tensor_prefetch_tile_5d;
  default:
    llvm_unreachable("Invalid TensorDim in CpAsyncBulkTensorPrefetchOp.");
  }
}

#define CP_ASYNC_BULK_TENSOR_REDUCE_MODE(op, dim, mode)                        \
  llvm::Intrinsic::nvvm_cp_async_bulk_tensor_##op##_##mode##_##dim##d

#define CP_ASYNC_BULK_TENSOR_REDUCE(op, dim, is_im2col)                        \
  is_im2col ? CP_ASYNC_BULK_TENSOR_REDUCE_MODE(op, dim, im2col)                \
            : CP_ASYNC_BULK_TENSOR_REDUCE_MODE(op, dim, tile)

#define GET_CP_ASYNC_BULK_TENSOR_ID(op, dims, is_im2col)                       \
  [&]() -> auto {                                                              \
    switch (dims) {                                                            \
    case 1:                                                                    \
      return CP_ASYNC_BULK_TENSOR_REDUCE_MODE(op, 1, tile);                    \
    case 2:                                                                    \
      return CP_ASYNC_BULK_TENSOR_REDUCE_MODE(op, 2, tile);                    \
    case 3:                                                                    \
      return CP_ASYNC_BULK_TENSOR_REDUCE(op, 3, is_im2col);                    \
    case 4:                                                                    \
      return CP_ASYNC_BULK_TENSOR_REDUCE(op, 4, is_im2col);                    \
    case 5:                                                                    \
      return CP_ASYNC_BULK_TENSOR_REDUCE(op, 5, is_im2col);                    \
    default:                                                                   \
      llvm_unreachable("Invalid TensorDim in CpAsyncBulkTensorReduceOp.");     \
    }                                                                          \
  }()

llvm::Intrinsic::ID CpAsyncBulkTensorReduceOp::getIntrinsicID(
    int tensorDims, NVVM::TMAReduxKind kind, bool isIm2Col) {
  using RedTy = NVVM::TMAReduxKind;
  switch (kind) {
  case RedTy::ADD:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_add, tensorDims, isIm2Col);
  case RedTy::MIN:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_min, tensorDims, isIm2Col);
  case RedTy::MAX:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_max, tensorDims, isIm2Col);
  case RedTy::INC:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_inc, tensorDims, isIm2Col);
  case RedTy::DEC:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_dec, tensorDims, isIm2Col);
  case RedTy::AND:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_and, tensorDims, isIm2Col);
  case RedTy::OR:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_or, tensorDims, isIm2Col);
  case RedTy::XOR:
    return GET_CP_ASYNC_BULK_TENSOR_ID(reduce_xor, tensorDims, isIm2Col);
  }
  llvm_unreachable("Invalid Reduction Op for CpAsyncBulkTensorReduceOp");
}

#define _none

#define CVT_F2TF32_ID_IMPL(rnd, relu, sf)                                      \
  hasRelu ? llvm::Intrinsic::nvvm_f2tf32_##rnd##relu##sf                       \
          : llvm::Intrinsic::nvvm_f2tf32_##rnd##sf

#define GET_CVT_F2TF32_ID(rnd, relu, sf)                                       \
  hasSatFinite ? CVT_F2TF32_ID_IMPL(rnd, relu, sf)                             \
               : CVT_F2TF32_ID_IMPL(rnd, relu, )

llvm::Intrinsic::ID
ConvertFloatToTF32Op::getIntrinsicID(NVVM::FPRoundingMode rnd,
                                     NVVM::SaturationMode sat, bool hasRelu) {
  using RndMode = NVVM::FPRoundingMode;
  bool hasSatFinite = (sat == NVVM::SaturationMode::SATFINITE);
  switch (rnd) {
  case RndMode::RN:
    return GET_CVT_F2TF32_ID(rn, _relu, _satfinite);
  case RndMode::RZ:
    return GET_CVT_F2TF32_ID(rz, _relu, _satfinite);
  case RndMode::RNA:
    return GET_CVT_F2TF32_ID(rna, _none, _satfinite);
  default:
    llvm_unreachable("Invalid RoundingMode for CvtFloatToTF32Op");
  }
}

#define GET_F32x2_TO_F6x2_ID(type, has_relu)                                   \
  has_relu ? llvm::Intrinsic::nvvm_ff_to_##type##_rn_relu_satfinite            \
           : llvm::Intrinsic::nvvm_ff_to_##type##_rn_satfinite

llvm::Intrinsic::ID
ConvertF32x2ToF6x2Op::getIntrinsicID(NVVM::ConvertFP6Type type, bool hasRelu) {
  switch (type) {
  case NVVM::ConvertFP6Type::E2M3:
    return GET_F32x2_TO_F6x2_ID(e2m3x2, hasRelu);
  case NVVM::ConvertFP6Type::E3M2:
    return GET_F32x2_TO_F6x2_ID(e3m2x2, hasRelu);
  }
  llvm_unreachable("Invalid conversion in ConvertF32x2ToF6x2Op");
}

#define GET_F32x2_TO_F8X2_US_ID(rnd, has_satf)                                 \
  has_satf ? llvm::Intrinsic::nvvm_ff_to_ue8m0x2_##rnd##_satfinite             \
           : llvm::Intrinsic::nvvm_ff_to_ue8m0x2_##rnd

#define GET_F32x2_TO_F8X2_S_ID(type, has_relu)                                 \
  has_relu ? llvm::Intrinsic::nvvm_ff_to_##type##_rn_relu                      \
           : llvm::Intrinsic::nvvm_ff_to_##type##_rn

llvm::Intrinsic::ID
ConvertF32x2ToF8x2Op::getIntrinsicID(NVVM::ConvertFP8Type type,
                                     NVVM::FPRoundingMode rnd,
                                     NVVM::SaturationMode sat, bool hasRelu) {
  bool hasSatFinite = (sat == NVVM::SaturationMode::SATFINITE);
  bool hasRoundingModeRZ = (rnd == NVVM::FPRoundingMode::RZ);
  bool hasRoundingModeRP = (rnd == NVVM::FPRoundingMode::RP);

  switch (type) {
  case NVVM::ConvertFP8Type::E4M3:
    return GET_F32x2_TO_F8X2_S_ID(e4m3x2, hasRelu);
  case NVVM::ConvertFP8Type::E5M2:
    return GET_F32x2_TO_F8X2_S_ID(e5m2x2, hasRelu);
  case NVVM::ConvertFP8Type::UE8M0:
    if (hasRoundingModeRZ)
      return GET_F32x2_TO_F8X2_US_ID(rz, hasSatFinite);
    else if (hasRoundingModeRP)
      return GET_F32x2_TO_F8X2_US_ID(rp, hasSatFinite);
  }
  llvm_unreachable("Invalid conversion in CvtFloatToF8x2Op");
}

#define GET_F16x2_TO_F8X2_ID(type, has_relu)                                   \
  has_relu ? llvm::Intrinsic::nvvm_f16x2_to_##type##_rn_relu                   \
           : llvm::Intrinsic::nvvm_f16x2_to_##type##_rn

llvm::Intrinsic::ID
ConvertF16x2ToF8x2Op::getIntrinsicID(NVVM::ConvertFP8Type type, bool hasRelu) {
  switch (type) {
  case NVVM::ConvertFP8Type::E4M3:
    return GET_F16x2_TO_F8X2_ID(e4m3x2, hasRelu);
  case NVVM::ConvertFP8Type::E5M2:
    return GET_F16x2_TO_F8X2_ID(e5m2x2, hasRelu);
  default:
    llvm_unreachable("Invalid ConvertFP8Type for CvtF16x2ToF8x2Op");
  }
}

#define GET_BF16X2_TO_F8X2_ID(rnd, has_satf)                                   \
  has_satf ? llvm::Intrinsic::nvvm_bf16x2_to_ue8m0x2_##rnd##_satfinite         \
           : llvm::Intrinsic::nvvm_bf16x2_to_ue8m0x2_##rnd

llvm::Intrinsic::ID
ConvertBF16x2ToF8x2Op::getIntrinsicID(NVVM::FPRoundingMode rnd,
                                      NVVM::SaturationMode sat) {
  bool hasSatFinite = (sat == NVVM::SaturationMode::SATFINITE);
  switch (rnd) {
  case NVVM::FPRoundingMode::RZ:
    return GET_BF16X2_TO_F8X2_ID(rz, hasSatFinite);
  case NVVM::FPRoundingMode::RP:
    return GET_BF16X2_TO_F8X2_ID(rp, hasSatFinite);
  default:
    llvm_unreachable("Invalid rounding mode for CvtBF16x2ToF8x2Op");
  }
}

llvm::Intrinsic::ID
Tcgen05AllocOp::getIntrinsicIDAndArgs(Operation &op,
                                      LLVM::ModuleTranslation &mt,
                                      llvm::SmallVector<llvm::Value *> &args) {
  auto curOp = cast<NVVM::Tcgen05AllocOp>(op);
  unsigned as = llvm::cast<LLVM::LLVMPointerType>(curOp.getAddr().getType())
                    .getAddressSpace();
  bool isShared = as == NVVMMemorySpace::kSharedMemorySpace;
  bool is2CTAMode = curOp.getGroup() == Tcgen05GroupKind::CTA_2;

  llvm::Intrinsic::ID id;
  if (isShared) {
    id = is2CTAMode ? llvm::Intrinsic::nvvm_tcgen05_alloc_shared_cg2
                    : llvm::Intrinsic::nvvm_tcgen05_alloc_shared_cg1;
  } else {
    id = is2CTAMode ? llvm::Intrinsic::nvvm_tcgen05_alloc_cg2
                    : llvm::Intrinsic::nvvm_tcgen05_alloc_cg1;
  }

  // Fill the Intrinsic Args
  args.push_back(mt.lookupValue(curOp.getAddr()));
  args.push_back(mt.lookupValue(curOp.getNCols()));

  return id;
}

llvm::Intrinsic::ID Tcgen05DeallocOp::getIntrinsicIDAndArgs(
    Operation &op, LLVM::ModuleTranslation &mt,
    llvm::SmallVector<llvm::Value *> &args) {
  auto curOp = cast<NVVM::Tcgen05DeallocOp>(op);
  auto id = (curOp.getGroup() == Tcgen05GroupKind::CTA_1)
                ? llvm::Intrinsic::nvvm_tcgen05_dealloc_cg1
                : llvm::Intrinsic::nvvm_tcgen05_dealloc_cg2;

  // Fill the Intrinsic Args
  args.push_back(mt.lookupValue(curOp.getTaddr()));
  args.push_back(mt.lookupValue(curOp.getNCols()));

  return id;
}

#define TCGEN05_COMMIT_IMPL(cg, is_shared, mc)                                 \
  is_shared ? llvm::Intrinsic::nvvm_tcgen05_commit##mc##_shared##_##cg         \
            : llvm::Intrinsic::nvvm_tcgen05_commit##mc##_##cg

#define GET_TCGEN05_COMMIT_ID(cta_group, is_shared, has_mc)                    \
  has_mc ? TCGEN05_COMMIT_IMPL(cta_group, is_shared, _mc)                      \
         : TCGEN05_COMMIT_IMPL(cta_group, is_shared, )

llvm::Intrinsic::ID
Tcgen05CommitOp::getIntrinsicIDAndArgs(Operation &op,
                                       LLVM::ModuleTranslation &mt,
                                       llvm::SmallVector<llvm::Value *> &args) {
  auto curOp = cast<NVVM::Tcgen05CommitOp>(op);
  unsigned as = llvm::cast<LLVM::LLVMPointerType>(curOp.getAddr().getType())
                    .getAddressSpace();
  bool isShared = as == NVVMMemorySpace::kSharedMemorySpace;
  bool hasMulticast = static_cast<bool>(curOp.getMulticastMask());
  bool is2CTAMode = curOp.getGroup() == Tcgen05GroupKind::CTA_2;

  llvm::Intrinsic::ID id =
      is2CTAMode ? GET_TCGEN05_COMMIT_ID(cg2, isShared, hasMulticast)
                 : GET_TCGEN05_COMMIT_ID(cg1, isShared, hasMulticast);

  // Fill the Intrinsic Args
  args.push_back(mt.lookupValue(curOp.getAddr()));
  if (hasMulticast)
    args.push_back(mt.lookupValue(curOp.getMulticastMask()));

  return id;
}

#define TCGEN05_CP_IMPL(shape_mc, src_fmt, cg)                                 \
  llvm::Intrinsic::nvvm_tcgen05_cp##shape_mc##src_fmt##cg

#define TCGEN05_CP_2CTA(shape_mc, src_fmt, is_2cta)                            \
  is_2cta ? TCGEN05_CP_IMPL(shape_mc, src_fmt, _cg2)                           \
          : TCGEN05_CP_IMPL(shape_mc, src_fmt, _cg1)

#define GET_TCGEN05_CP_ID(shape_mc, src_fmt, is_2cta)                          \
  [&]() -> auto {                                                              \
    if ((src_fmt) == Tcgen05CpSrcFormat::B6x16_P32)                            \
      return TCGEN05_CP_2CTA(shape_mc, _b6x16_p32, is_2cta);                   \
    if ((src_fmt) == Tcgen05CpSrcFormat::B4x16_P64)                            \
      return TCGEN05_CP_2CTA(shape_mc, _b4x16_p64, is_2cta);                   \
    return TCGEN05_CP_2CTA(shape_mc, , is_2cta);                               \
  }()

llvm::Intrinsic::ID Tcgen05CpOp::getIntrinsicID(Operation &op) {
  auto curOp = cast<NVVM::Tcgen05CpOp>(op);
  bool is2CTA = curOp.getGroup() == Tcgen05GroupKind::CTA_2;
  auto srcFmt = curOp.getSrcFormat();
  auto mc = curOp.getMulticast();

  switch (curOp.getShape()) {
  case Tcgen05CpShape::SHAPE_128x256b:
    return GET_TCGEN05_CP_ID(_128x256b, srcFmt, is2CTA);
  case Tcgen05CpShape::SHAPE_128x128b:
    return GET_TCGEN05_CP_ID(_128x128b, srcFmt, is2CTA);
  case Tcgen05CpShape::SHAPE_4x256b:
    return GET_TCGEN05_CP_ID(_4x256b, srcFmt, is2CTA);
  case Tcgen05CpShape::SHAPE_32x128b:
    return GET_TCGEN05_CP_ID(_32x128b_warpx4, srcFmt, is2CTA);
  case Tcgen05CpShape::SHAPE_64x128b:
    return (mc == Tcgen05CpMulticast::WARPX2_01_23)
               ? GET_TCGEN05_CP_ID(_64x128b_warpx2_01_23, srcFmt, is2CTA)
               : GET_TCGEN05_CP_ID(_64x128b_warpx2_02_13, srcFmt, is2CTA);
  }
  llvm_unreachable("Invalid shape in tcgen05 cp Op");
}

// Returns the valid vector length for a given shape and vector length, the
// function models the table mentioned in the tcgen05.{ld, st} Op description
static unsigned isValidVectorLength(NVVM::Tcgen05LdStShape shape,
                                    unsigned vecLen) {
  if (shape == NVVM::Tcgen05LdStShape::SHAPE_16X128B)
    return vecLen >= 2;
  if (shape == NVVM::Tcgen05LdStShape::SHAPE_16X256B)
    return vecLen >= 4;
  return true;
}

LogicalResult Tcgen05LdOp::verify() {
  LogicalResult result = success();
  if (getShape() == NVVM::Tcgen05LdStShape::SHAPE_16X32BX2 && !getOffset())
    result = emitError("shape 16x32bx2 requires offset argument");

  auto resTy = getRes().getType();
  unsigned resLen = isa<VectorType>(resTy)
                        ? llvm::cast<VectorType>(resTy).getNumElements()
                        : 1;
  if (!isValidVectorLength(getShape(), resLen))
    result = emitError(llvm::formatv("invalid result type length {0} for shape "
                                     "{1} in tcgen05.ld Op",
                                     resLen, stringifyEnum(getShape())));

  return result;
}

LogicalResult Tcgen05StOp::verify() {
  LogicalResult result = success();
  if (getShape() == NVVM::Tcgen05LdStShape::SHAPE_16X32BX2 && !getOffset())
    result = emitError("shape 16x32bx2 requires offset argument");

  auto valTy = getVal().getType();
  unsigned valLen = isa<VectorType>(valTy)
                        ? llvm::cast<VectorType>(valTy).getNumElements()
                        : 1;
  if (!isValidVectorLength(getShape(), valLen))
    result = emitError(llvm::formatv("invalid input length {0} for shape "
                                     "{1} in tcgen05.st Op",
                                     valLen, stringifyEnum(getShape())));

  return result;
}

/// Infer the result ranges for the NVVM SpecialRangeableRegisterOp that might
/// have ConstantRangeAttr.
static void nvvmInferResultRanges(Operation *op, Value result,
                                  ArrayRef<::mlir::ConstantIntRanges> argRanges,
                                  SetIntRangeFn setResultRanges) {
  if (auto rangeAttr = op->getAttrOfType<LLVM::ConstantRangeAttr>("range")) {
    setResultRanges(result, {rangeAttr.getLower(), rangeAttr.getUpper(),
                             rangeAttr.getLower(), rangeAttr.getUpper()});
  }
}

static llvm::Value *getAsPackedI32(llvm::Value *arg,
                                   llvm::IRBuilderBase &builder) {
  return builder.CreateBitCast(arg,
                               llvm::Type::getInt32Ty(builder.getContext()));
}

NVVM::IDArgPair DotAccumulate4WayOp::getIntrinsicIDAndArgs(
    Operation &op, LLVM::ModuleTranslation &mt, llvm::IRBuilderBase &builder) {
  auto curOp = cast<NVVM::DotAccumulate4WayOp>(op);

  llvm::SmallVector<llvm::Value *> args;
  args.push_back(getAsPackedI32(mt.lookupValue(curOp.getA()), builder));
  args.push_back(getAsPackedI32(mt.lookupValue(curOp.getB()), builder));
  args.push_back(mt.lookupValue(curOp.getC()));

  bool isASigned = curOp.getAType() == NVVM::DotAccumulateType::SIGNED;
  bool isBSigned = curOp.getBType() == NVVM::DotAccumulateType::SIGNED;
  unsigned type = (isASigned << 1) | isBSigned;
  const llvm::Intrinsic::ID ids[] = {
      llvm::Intrinsic::nvvm_idp4a_u_u,
      llvm::Intrinsic::nvvm_idp4a_u_s,
      llvm::Intrinsic::nvvm_idp4a_s_u,
      llvm::Intrinsic::nvvm_idp4a_s_s,
  };
  return {ids[type], args};
}

NVVM::IDArgPair DotAccumulate2WayOp::getIntrinsicIDAndArgs(
    Operation &op, LLVM::ModuleTranslation &mt, llvm::IRBuilderBase &builder) {
  auto curOp = cast<NVVM::DotAccumulate2WayOp>(op);

  llvm::SmallVector<llvm::Value *> args;
  args.push_back(getAsPackedI32(mt.lookupValue(curOp.getA()), builder));
  args.push_back(getAsPackedI32(mt.lookupValue(curOp.getB()), builder));
  args.push_back(builder.getInt1(curOp.getBHi()));
  args.push_back(mt.lookupValue(curOp.getC()));

  bool isASigned = curOp.getAType() == NVVM::DotAccumulateType::SIGNED;
  bool isBSigned = curOp.getBType() == NVVM::DotAccumulateType::SIGNED;
  unsigned type = (isASigned << 1) | isBSigned;
  const llvm::Intrinsic::ID ids[] = {
      llvm::Intrinsic::nvvm_idp2a_u_u,
      llvm::Intrinsic::nvvm_idp2a_u_s,
      llvm::Intrinsic::nvvm_idp2a_s_u,
      llvm::Intrinsic::nvvm_idp2a_s_s,
  };
  return {ids[type], args};
}

llvm::Intrinsic::ID PrefetchOp::getIntrinsicID(NVVM::PrefetchOp &op) {
  using MemSpace = NVVM::NVVMMemorySpace;
  using CacheLevel = NVVM::PrefetchCacheLevel;

  NVVM::PrefetchCacheLevel cacheLevel = op.getCacheLevel();
  std::optional<NVVM::CacheEvictionPriority> evictPriority =
      op.getEvictPriority();
  unsigned addressSpace =
      llvm::cast<LLVM::LLVMPointerType>(op.getAddr().getType())
          .getAddressSpace();

  if (op.getUniform() && cacheLevel == CacheLevel::L1)
    return llvm::Intrinsic::nvvm_prefetchu_L1;

  if (evictPriority && cacheLevel == CacheLevel::L2) {
    switch (*evictPriority) {
    case NVVM::CacheEvictionPriority::EvictLast:
      return llvm::Intrinsic::nvvm_prefetch_global_L2_evict_last;
    case NVVM::CacheEvictionPriority::EvictNormal:
      return llvm::Intrinsic::nvvm_prefetch_global_L2_evict_normal;
    default:
      llvm_unreachable("Invalid cache eviction priority");
    }
  }

  switch (addressSpace) {
  case MemSpace::kGenericMemorySpace:
    return cacheLevel == CacheLevel::L1 ? llvm::Intrinsic::nvvm_prefetch_L1
                                        : llvm::Intrinsic::nvvm_prefetch_L2;
  case MemSpace::kGlobalMemorySpace:
    return cacheLevel == CacheLevel::L1
               ? llvm::Intrinsic::nvvm_prefetch_global_L1
               : llvm::Intrinsic::nvvm_prefetch_global_L2;
  case MemSpace::kLocalMemorySpace:
    return cacheLevel == CacheLevel::L1
               ? llvm::Intrinsic::nvvm_prefetch_local_L1
               : llvm::Intrinsic::nvvm_prefetch_local_L2;
  default:
    llvm_unreachable("Invalid pointer address space");
  }
}

//===----------------------------------------------------------------------===//
// NVVMDialect initialization, type parsing, and registration.
//===----------------------------------------------------------------------===//

// TODO: This should be the llvm.nvvm dialect once this is supported.
void NVVMDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "mlir/Dialect/LLVMIR/NVVMOps.cpp.inc"
      >();
  addAttributes<
#define GET_ATTRDEF_LIST
#include "mlir/Dialect/LLVMIR/NVVMOpsAttributes.cpp.inc"
      >();

  // Support unknown operations because not all NVVM operations are
  // registered.
  allowUnknownOperations();
  declarePromisedInterface<ConvertToLLVMPatternInterface, NVVMDialect>();
  declarePromisedInterface<gpu::TargetAttrInterface, NVVMTargetAttr>();
}

LogicalResult NVVMDialect::verifyOperationAttribute(Operation *op,
                                                    NamedAttribute attr) {
  StringAttr attrName = attr.getName();
  // Kernel function attribute should be attached to functions.
  if (attrName == NVVMDialect::getKernelFuncAttrName()) {
    if (!isa<LLVM::LLVMFuncOp>(op)) {
      return op->emitError() << "'" << NVVMDialect::getKernelFuncAttrName()
                             << "' attribute attached to unexpected op";
    }
  }
  // If maxntid / reqntid / cluster_dim exist, it must be an array with max 3
  // dim
  if (attrName == NVVMDialect::getMaxntidAttrName() ||
      attrName == NVVMDialect::getReqntidAttrName() ||
      attrName == NVVMDialect::getClusterDimAttrName()) {
    auto values = llvm::dyn_cast<DenseI32ArrayAttr>(attr.getValue());
    if (!values || values.empty() || values.size() > 3)
      return op->emitError()
             << "'" << attrName
             << "' attribute must be integer array with maximum 3 index";
  }
  // If minctasm / maxnreg / cluster_max_blocks exist, it must be an integer
  // attribute
  if (attrName == NVVMDialect::getMinctasmAttrName() ||
      attrName == NVVMDialect::getMaxnregAttrName() ||
      attrName == NVVMDialect::getClusterMaxBlocksAttrName()) {
    if (!llvm::dyn_cast<IntegerAttr>(attr.getValue()))
      return op->emitError()
             << "'" << attrName << "' attribute must be integer constant";
  }

  return success();
}

LogicalResult NVVMDialect::verifyRegionArgAttribute(Operation *op,
                                                    unsigned regionIndex,
                                                    unsigned argIndex,
                                                    NamedAttribute argAttr) {
  auto funcOp = dyn_cast<FunctionOpInterface>(op);
  if (!funcOp)
    return success();

  bool isKernel = op->hasAttr(NVVMDialect::getKernelFuncAttrName());
  StringAttr attrName = argAttr.getName();
  if (attrName == NVVM::NVVMDialect::getGridConstantAttrName()) {
    if (!isKernel) {
      return op->emitError()
             << "'" << attrName
             << "' attribute must be present only on kernel arguments";
    }
    if (!isa<UnitAttr>(argAttr.getValue()))
      return op->emitError() << "'" << attrName << "' must be a unit attribute";
    if (!funcOp.getArgAttr(argIndex, LLVM::LLVMDialect::getByValAttrName())) {
      return op->emitError()
             << "'" << attrName
             << "' attribute requires the argument to also have attribute '"
             << LLVM::LLVMDialect::getByValAttrName() << "'";
    }
  }

  return success();
}

//===----------------------------------------------------------------------===//
// NVVM target attribute.
//===----------------------------------------------------------------------===//
LogicalResult
NVVMTargetAttr::verify(function_ref<InFlightDiagnostic()> emitError,
                       int optLevel, StringRef triple, StringRef chip,
                       StringRef features, DictionaryAttr flags,
                       ArrayAttr files, bool verifyTarget) {
  if (optLevel < 0 || optLevel > 3) {
    emitError() << "The optimization level must be a number between 0 and 3.";
    return failure();
  }
  if (triple.empty()) {
    emitError() << "The target triple cannot be empty.";
    return failure();
  }
  if (chip.empty()) {
    emitError() << "The target chip cannot be empty.";
    return failure();
  }
  if (files && !llvm::all_of(files, [](::mlir::Attribute attr) {
        return mlir::isa_and_nonnull<StringAttr>(attr);
      })) {
    emitError() << "All the elements in the `link` array must be strings.";
    return failure();
  }
  return success();
}

LogicalResult NVVMTargetAttr::verifyTarget(Operation *gpuModule) {
  if (!getVerifyTarget())
    return success();

  auto gpuModuleOp = llvm::dyn_cast<gpu::GPUModuleOp>(gpuModule);
  if (!gpuModuleOp) {
    return emitError(gpuModule->getLoc(),
                     "NVVM target attribute must be attached to a GPU module");
  }

  const NVVMCheckSMVersion targetSMVersion =
      NVVMCheckSMVersion::getTargetSMVersionFromStr(getChip());
  if (!targetSMVersion.isMinimumSMVersion()) {
    return emitError(gpuModule->getLoc(),
                     "Minimum NVVM target SM version is sm_20");
  }

  gpuModuleOp->walk([&](Operation *op) {
    if (auto reqOp = llvm::dyn_cast<NVVM::RequiresSMInterface>(op)) {
      const NVVMCheckSMVersion requirement = reqOp.getRequiredMinSMVersion();
      if (!requirement.isCompatibleWith(targetSMVersion)) {
        op->emitOpError() << "is not supported on " << getChip();
        return WalkResult::interrupt();
      }
    }
    return WalkResult::advance();
  });

  return success();
}

#define GET_OP_CLASSES
#include "mlir/Dialect/LLVMIR/NVVMOps.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "mlir/Dialect/LLVMIR/NVVMOpsAttributes.cpp.inc"
