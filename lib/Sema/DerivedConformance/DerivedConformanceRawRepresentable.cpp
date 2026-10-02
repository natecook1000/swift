//===--- DerivedConformanceRawRepresentable.cpp -----------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2025 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  This file implements implicit derivation of the RawRepresentable protocol
//  for an enum.
//
//===----------------------------------------------------------------------===//

#include "CodeSynthesis.h"
#include "DerivedConformance.h"
#include "TypeCheckAvailability.h"
#include "TypeCheckDecl.h"
#include "TypeChecker.h"
#include "swift/AST/AvailabilityQuery.h"
#include "swift/AST/AvailabilitySpec.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Expr.h"
#include "swift/AST/ParameterList.h"
#include "swift/AST/Pattern.h"
#include "swift/AST/PluginLoader.h"
#include "swift/AST/Stmt.h"
#include "swift/AST/Types.h"
#include "swift/Basic/QuotedString.h"

using namespace swift;

static LiteralExpr *cloneRawLiteralExpr(ASTContext &C, LiteralExpr *expr) {
  LiteralExpr *clone;
  if (auto intLit = dyn_cast<IntegerLiteralExpr>(expr)) {
    clone = new (C) IntegerLiteralExpr(intLit->getDigitsText(), expr->getLoc(),
                                       /*implicit*/ true);
    if (intLit->isNegative())
      cast<IntegerLiteralExpr>(clone)->setNegative(expr->getLoc());
  } else if (isa<NilLiteralExpr>(expr)) {
    clone = new (C) NilLiteralExpr(expr->getLoc());
  } else if (auto stringLit = dyn_cast<StringLiteralExpr>(expr)) {
    clone = new (C) StringLiteralExpr(stringLit->getValue(), expr->getLoc());
  } else if (auto floatLit = dyn_cast<FloatLiteralExpr>(expr)) {
    clone = new (C) FloatLiteralExpr(floatLit->getDigitsText(), expr->getLoc(),
                                     /*implicit*/ true);
    if (floatLit->isNegative())
      cast<FloatLiteralExpr>(clone)->setNegative(expr->getLoc());
  } else if (auto boolLit = dyn_cast<BooleanLiteralExpr>(expr)) {
    clone = new (C) BooleanLiteralExpr(boolLit->getValue(), expr->getLoc(),
                                       /*implicit*/true);
  } else {
    llvm_unreachable("invalid raw literal expr");
  }
  clone->setImplicit();
  return clone;
}

static Type deriveRawRepresentable_Raw(DerivedConformance &derived) {
  // enum SomeEnum : SomeType {
  //   @derived
  //   typealias Raw = SomeType
  // }
  auto rawInterfaceType = cast<EnumDecl>(derived.Nominal)->getRawType();
  return derived.getConformanceContext()->mapTypeIntoEnvironment(rawInterfaceType);
}

static std::pair<BraceStmt *, bool>
deriveBodyRawRepresentable_raw(AbstractFunctionDecl *toRawDecl, void *) {
  // enum SomeEnum : SomeType {
  //   case A = 111, B = 222
  //   @derived
  //   var raw: SomeType {
  //     switch self {
  //     case A:
  //       return 111
  //     case B:
  //       return 222
  //     }
  //   }
  // }

  auto parentDC = toRawDecl->getDeclContext();
  ASTContext &C = parentDC->getASTContext();

  auto enumDecl = parentDC->getSelfEnumDecl();

  Type rawTy = enumDecl->getRawType();
  assert(rawTy);
  rawTy = toRawDecl->mapTypeIntoEnvironment(rawTy);

  if (enumDecl->isObjC()) {
    // Special case: ObjC enums are represented by their raw value, so just use
    // a bitcast.

    // return unsafeBitCast(self, to: RawType.self)
    auto functionRef = UnresolvedDeclRefExpr::createImplicit(
        C, C.getIdentifier("unsafeBitCast"), {Identifier(), C.Id_to});
    auto selfRef = DerivedConformance::createSelfDeclRef(toRawDecl);
    auto bareTypeExpr = TypeExpr::createImplicit(rawTy, C);
    auto typeExpr = new (C) DotSelfExpr(bareTypeExpr, SourceLoc(), SourceLoc());

    auto *argList = ArgumentList::forImplicitCallTo(functionRef->getName(),
                                                    {selfRef, typeExpr}, C);
    Expr *call = CallExpr::createImplicit(C, functionRef, argList);
    if (C.LangOpts.hasFeature(Feature::StrictMemorySafety, /*allowMigration=*/true))
      call = UnsafeExpr::createImplicit(C, SourceLoc(), call);
    auto *returnStmt = ReturnStmt::createImplicit(C, call);
    auto body = BraceStmt::create(C, SourceLoc(), ASTNode(returnStmt),
                                  SourceLoc());
    return { body, /*isTypeChecked=*/false };
  }

  Type enumType = parentDC->getDeclaredTypeInContext();

  SmallVector<CaseStmt *, 4> cases;
  for (auto elt : enumDecl->getAllElements()) {
    auto *pat = EnumElementPattern::createImplicit(
        enumType, elt, /*subPattern*/ nullptr, /*DC*/ toRawDecl);

    auto labelItem = CaseLabelItem(pat);

    auto returnExpr = cloneRawLiteralExpr(C, elt->getRawValueExpr());
    auto *returnStmt = ReturnStmt::createImplicit(C, returnExpr);

    auto body = BraceStmt::create(C, SourceLoc(),
                                  ASTNode(returnStmt), SourceLoc());

    cases.push_back(
        CaseStmt::createImplicit(C, CaseParentKind::Switch, labelItem, body));
  }

  auto selfRef = DerivedConformance::createSelfDeclRef(toRawDecl);
  auto switchStmt =
      SwitchStmt::createImplicit(LabeledStmtInfo(), selfRef, cases, C);
  auto body = BraceStmt::create(C, SourceLoc(),
                                ASTNode(switchStmt),
                                SourceLoc());
  return { body, /*isTypeChecked=*/false };
}

static void maybeMarkAsInlinable(DerivedConformance &derived,
                                 AbstractFunctionDecl *afd) {
  ASTContext &C = derived.Context;
  auto parentDC = derived.getConformanceContext();
  if (!parentDC->getParentModule()->isResilient()) {
    AccessScope access =
        afd->getFormalAccessScope(nullptr,
                                  /*treatUsableFromInlineAsPublic*/true);
    if (auto *attr = afd->getAttrs().getAttribute<UsableFromInlineAttr>())
      attr->setInvalid();
    if (access.isPublic())
      afd->addAttribute(new (C) InlinableAttr(/*implicit*/ false));
  }
}

static VarDecl *deriveRawRepresentable_raw(DerivedConformance &derived) {
  ASTContext &C = derived.Context;

  auto enumDecl = cast<EnumDecl>(derived.Nominal);
  auto rawInterfaceType = enumDecl->getRawType();

  // Define the property.
  VarDecl *propDecl;
  PatternBindingDecl *pbDecl;
  std::tie(propDecl, pbDecl) = derived.declareDerivedProperty(
      DerivedConformance::SynthesizedIntroducer::Var, C.Id_rawValue,
      rawInterfaceType, /*isStatic=*/false, /*isFinal=*/false);
  addNonIsolatedToSynthesized(enumDecl, propDecl);

  // Define the getter.
  auto getterDecl =
      DerivedConformance::addGetterToReadOnlyDerivedProperty(propDecl);
  getterDecl->setBodySynthesizer(&deriveBodyRawRepresentable_raw);

  // If the containing module is not resilient, make sure clients can get at
  // the raw value without function call overhead.
  maybeMarkAsInlinable(derived, getterDecl);

  derived.addMembersToConformanceContext({propDecl, pbDecl});

  return propDecl;
}

/// Synthesizes a statement which returns nil if the given availability query
/// fails, e.g. "guard #available(iOS 10, *) else { return nil }".
static Stmt *createAvailabilityGuardStmt(ASTContext &C,
                                         const AvailabilityQuery &query) {
  auto domain = query.getDomain();
  bool isUnavailability = query.isUnavailability();

  // domainSpec = "\(domain) \(version)", e.g. "iOS 10". Domains that don't
  // support versioned availability have no version, e.g. "MyDomain".
  llvm::VersionTuple version;
  if (auto primaryRange = query.getPrimaryRange()) {
    if (primaryRange->hasMinimumVersion())
      version = primaryRange->getRawMinimumVersion();
  }
  auto domainSpec = AvailabilitySpec::createForDomain(C, domain, SourceLoc(),
                                                      version, SourceLoc());

  SmallVector<AvailabilitySpec *, 2> specs;
  specs.push_back(domainSpec);

  // Availability in some domains, like custom domains, must be specified alone,
  // and an "#unavailable" query never takes a wildcard. For the rest, add the
  // wildcard spec "*".
  if (!isUnavailability && !domain.mustBeSpecifiedAlone())
    specs.push_back(AvailabilitySpec::createWildcard(C, SourceLoc()));

  // availableInfo = "#available(\(specs))", or "#unavailable(\(specs))"
  auto availableInfo = PoundAvailableInfo::create(
      C, SourceLoc(), SourceLoc(), specs, SourceLoc(), isUnavailability);

  // This won't be filled in by TypeCheckAvailability because synthesized code
  // has invalid source locations.
  availableInfo->setAvailabilityQuery(query);

  // earlyReturnBody = "{ return nil }"
  auto earlyReturn = new (C) FailStmt(SourceLoc(), SourceLoc());
  auto earlyReturnBody = BraceStmt::create(C, SourceLoc(), ASTNode(earlyReturn),
                                           SourceLoc(), /*implicit=*/true);

  // guardStmt = "guard \(availableInfo) else \(earlyReturnBody)"
  StmtConditionElement conds[1] = {availableInfo};
  auto guardStmt = new (C) GuardStmt(SourceLoc(), C.AllocateCopy(conds),
                                     earlyReturnBody, /*implicit=*/true);

  return guardStmt;
}

/// The largest number of cases whose tag still fits in a single byte.
static constexpr unsigned MaxContiguousRawValueCases = 256;

static bool isSignedStdlibInteger(Type rawTy) {
  return rawTy->isInt() || rawTy->isInt8() || rawTy->isInt16() ||
         rawTy->isInt32() || rawTy->isInt64() || rawTy->isInt128();
}

/// If each case of \p enumDecl has a raw value equal to its own
/// declaration-order index, returns the number of cases. Such an enum's tag is
/// already its raw value, so `init?(rawValue:)` can be synthesized as a range
/// check rather than a comparison per case.
static std::optional<unsigned>
getContiguousRawValueCaseCount(EnumDecl *enumDecl, Type rawTy,
                               AvailabilityContext availabilityContext) {
  // A non-integer raw type bears no relation to the tag, even when its raw
  // values are written as integer literals.
  if (!rawTy->isStdlibInteger())
    return std::nullopt;

  // Imported and @objc enums are represented by their raw value rather than by
  // a case index.
  if (enumDecl->hasClangNode() || enumDecl->isObjC())
    return std::nullopt;

  unsigned caseCount = 0;
  for (auto elt : enumDecl->getAllElements()) {
    // An unavailable case still consumes its index, and a conditionally
    // available one needs a runtime query, so neither can be reached by an
    // unguarded conversion.
    SmallVector<AvailabilityQuery, 1> availabilityQueries;
    if (!checkAvailabilityForElement(elt, availabilityContext,
                                     availabilityQueries))
      return std::nullopt;
    if (!availabilityQueries.empty())
      return std::nullopt;

    auto litExpr = dyn_cast_or_null<IntegerLiteralExpr>(elt->getRawValueExpr());
    if (!litExpr || litExpr->isNegative())
      return std::nullopt;

    // The literal may be written in any radix, so compare its value rather
    // than its text.
    auto rawValue = litExpr->getRawValue();
    if (rawValue.getActiveBits() > 32 || rawValue.getZExtValue() != caseCount)
      return std::nullopt;

    if (++caseCount > MaxContiguousRawValueCases)
      return std::nullopt;
  }

  // A single-case enum is zero-sized, so it has no tag byte to convert.
  if (caseCount < 2)
    return std::nullopt;

  return caseCount;
}

static std::pair<BraceStmt *, bool>
deriveBodyRawRepresentable_contiguousInit(AbstractFunctionDecl *initDecl,
                                          Type rawTy, unsigned caseCount) {
  // enum SomeEnum : SomeType {
  //   case A, B, C
  //   @derived
  //   init?(rawValue: SomeType) {
  //     guard rawValue >= 0, rawValue <= 2 else { return nil }
  //     self = unsafeBitCast(UInt8(truncatingIfNeeded: rawValue),
  //                          to: Self.self)
  //   }
  // }

  auto parentDC = initDecl->getDeclContext();
  ASTContext &C = parentDC->getASTContext();

  Type enumType = parentDC->getDeclaredTypeInContext();
  auto selfDecl = cast<ConstructorDecl>(initDecl)->getImplicitSelfDecl();
  auto rawDecl = initDecl->getParameters()->get(0);

  SmallVector<StmtConditionElement, 2> conditions;

  // A signed raw type can hold values that an upper bound alone would admit.
  if (isSignedStdlibInteger(rawTy)) {
    // lowerBound = "rawValue >= 0"
    auto geFuncExpr = new (C)
        UnresolvedDeclRefExpr(DeclNameRef(C.getIdentifier(">=")),
                              DeclRefKind::BinaryOperator, DeclNameLoc());
    auto rawRef =
        new (C) DeclRefExpr(rawDecl, DeclNameLoc(), /*implicit*/ true);
    auto zero = IntegerLiteralExpr::createFromUnsigned(C, 0, SourceLoc());
    auto lowerBound = BinaryExpr::create(C, rawRef, geFuncExpr, zero,
                                         /*implicit*/ true);
    conditions.emplace_back(lowerBound);
  }

  // upperBound = "rawValue <= \(caseCount - 1)". The bound is the last case's
  // own raw value, which unlike the case count is always representable in the
  // raw type.
  auto leFuncExpr =
      new (C) UnresolvedDeclRefExpr(DeclNameRef(C.getIdentifier("<=")),
                                    DeclRefKind::BinaryOperator, DeclNameLoc());
  auto rawRef = new (C) DeclRefExpr(rawDecl, DeclNameLoc(), /*implicit*/ true);
  auto lastValue =
      IntegerLiteralExpr::createFromUnsigned(C, caseCount - 1, SourceLoc());
  auto upperBound = BinaryExpr::create(C, rawRef, leFuncExpr, lastValue,
                                       /*implicit*/ true);
  conditions.emplace_back(upperBound);

  // guardStmt = "guard \(conditions) else { return nil }"
  auto failStmt = new (C) FailStmt(SourceLoc(), SourceLoc());
  auto failBody = BraceStmt::create(C, SourceLoc(), ASTNode(failStmt),
                                    SourceLoc(), /*implicit=*/true);
  auto guardStmt = new (C) GuardStmt(SourceLoc(), C.AllocateCopy(conditions),
                                     failBody, /*implicit=*/true);

  // tagExpr = "UInt8(truncatingIfNeeded: rawValue)"
  auto tagTypeExpr = TypeExpr::createImplicit(C.getUInt8Type(), C);
  auto tagRawRef = new (C) DeclRefExpr(rawDecl, DeclNameLoc(),
                                       /*implicit*/ true);
  Argument tagArgs[] = {
      Argument(SourceLoc(), C.getIdentifier("truncatingIfNeeded"), tagRawRef)};
  auto tagExpr = CallExpr::createImplicit(
      C, tagTypeExpr, ArgumentList::createImplicit(C, tagArgs));

  // call = "unsafeBitCast(\(tagExpr), to: Self.self)"
  auto functionRef = UnresolvedDeclRefExpr::createImplicit(
      C, C.getIdentifier("unsafeBitCast"), {Identifier(), C.Id_to});
  auto bareTypeExpr = TypeExpr::createImplicit(enumType, C);
  auto typeExpr = new (C) DotSelfExpr(bareTypeExpr, SourceLoc(), SourceLoc());
  auto *argList = ArgumentList::forImplicitCallTo(functionRef->getName(),
                                                  {tagExpr, typeExpr}, C);
  Expr *call = CallExpr::createImplicit(C, functionRef, argList);
  if (C.LangOpts.hasFeature(Feature::StrictMemorySafety,
                            /*allowMigration=*/true))
    call = UnsafeExpr::createImplicit(C, SourceLoc(), call);

  // assignment = "self = \(call)"
  auto selfRef =
      new (C) DeclRefExpr(selfDecl, DeclNameLoc(),
                          /*implicit*/ true, AccessSemantics::DirectToStorage);
  auto assignment = new (C) AssignExpr(selfRef, SourceLoc(), call,
                                       /*implicit*/ true);

  ASTNode stmts[2] = {ASTNode(guardStmt), ASTNode(assignment)};
  auto body = BraceStmt::create(C, SourceLoc(), stmts, SourceLoc());
  return {body, /*isTypeChecked=*/false};
}

static std::pair<BraceStmt *, bool>
deriveBodyRawRepresentable_init(AbstractFunctionDecl *initDecl, void *) {
  // enum SomeEnum : SomeType {
  //   case A = 111, B = 222
  //   @available(iOS 10, *) case C = 333
  //   @derived
  //   init?(rawValue: SomeType) {
  //     switch rawValue {
  //     case 111:
  //       self = .A
  //     case 222:
  //       self = .B
  //     case 333:
  //       guard #available(iOS 10, *) else { return nil }
  //       self = .C
  //     default:
  //       return nil
  //     }
  //   }
  // }
  
  auto parentDC = initDecl->getDeclContext();
  ASTContext &C = parentDC->getASTContext();
  auto availabilityContext = AvailabilityContext::forDeploymentTarget(C);

  auto nominalTypeDecl = parentDC->getSelfNominalTypeDecl();
  auto enumDecl = cast<EnumDecl>(nominalTypeDecl);

  Type rawTy = enumDecl->getRawType();
  assert(rawTy);
  rawTy = initDecl->mapTypeIntoEnvironment(rawTy);

  bool isStringEnum = rawTy->isString();
  llvm::SmallVector<Expr *, 16> stringExprs;

  // When the raw values are the contiguous case indices, the tag is already
  // the raw value, so a range check stands in for the switch below.
  if (auto caseCount =
          getContiguousRawValueCaseCount(enumDecl, rawTy, availabilityContext))
    return deriveBodyRawRepresentable_contiguousInit(initDecl, rawTy,
                                                     *caseCount);

  Type enumType = parentDC->getDeclaredTypeInContext();

  auto selfDecl = cast<ConstructorDecl>(initDecl)->getImplicitSelfDecl();

  SmallVector<CaseStmt *, 4> cases;
  unsigned Idx = 0;
  for (auto elt : enumDecl->getAllElements()) {
    // First, check case availability. If the case will definitely be
    // unavailable, skip it. If it might be unavailable at runtime, save the
    // queries it needs in availabilityQueries and keep processing this element.
    SmallVector<AvailabilityQuery, 1> availabilityQueries;
    if (!checkAvailabilityForElement(elt, availabilityContext,
                                     availabilityQueries))
      continue;

    // litPat = elt.rawValueExpr as a pattern
    LiteralExpr *litExpr = cloneRawLiteralExpr(C, elt->getRawValueExpr());
    if (isStringEnum) {
      // In case of a string enum we are calling the _findStringSwitchCase
      // function from the library and switching on the returned Int value.
      stringExprs.push_back(litExpr);
      litExpr = IntegerLiteralExpr::createFromUnsigned(C, Idx, SourceLoc()); 
    }
    auto *litPat = ExprPattern::createImplicit(C, litExpr, /*DC*/ initDecl);

    /// Statements in the body of this case.
    SmallVector<ASTNode, 2> stmts;

    // If checkAvailability() discovered we need runtime availability queries,
    // add them now.
    for (auto const &query : availabilityQueries)
      stmts.push_back(ASTNode(createAvailabilityGuardStmt(C, query)));

    // Create a statement which assigns the case to self.

    // valueExpr = "\(enumType).\(elt)"
    auto metaTyRef = TypeExpr::createImplicit(enumType, C);
    auto valueExpr = new (C) MemberRefExpr(metaTyRef, SourceLoc(),
                                           elt, DeclNameLoc(), /*implicit*/true);
    
    // assignment = "self = \(valueExpr)"
    auto selfRef = new (C) DeclRefExpr(selfDecl, DeclNameLoc(),
                                       /*implicit*/true,
                                       AccessSemantics::DirectToStorage);
    auto assignment = new (C) AssignExpr(selfRef, SourceLoc(), valueExpr,
                                         /*implicit*/ true);

    stmts.push_back(ASTNode(assignment));
    
    // body = "{ \(stmts) }" (the braces are silent)
    auto body = BraceStmt::create(C, SourceLoc(),
                                  stmts, SourceLoc());

    // cases.append("case \(litPat): \(body)")
    cases.push_back(CaseStmt::createImplicit(C, CaseParentKind::Switch,
                                             CaseLabelItem(litPat), body));
    ++Idx;
  }

  auto anyPat = AnyPattern::createImplicit(C);
  auto dfltLabelItem = CaseLabelItem::getDefault(anyPat);

  auto dfltReturnStmt = new (C) FailStmt(SourceLoc(), SourceLoc());
  auto dfltBody = BraceStmt::create(C, SourceLoc(),
                                    ASTNode(dfltReturnStmt), SourceLoc());
  cases.push_back(CaseStmt::createImplicit(C, CaseParentKind::Switch,
                                           dfltLabelItem, dfltBody));

  auto rawDecl = initDecl->getParameters()->get(0);
  auto rawRef = new (C) DeclRefExpr(rawDecl, DeclNameLoc(), /*implicit*/true);
  Expr *switchArg = rawRef;
  if (isStringEnum) {
    // Call _findStringSwitchCase with an array of strings as argument.
    auto *Fun = UnresolvedDeclRefExpr::createImplicit(
        C, C.getIdentifier("_findStringSwitchCase"));
    auto *strArray = ArrayExpr::create(C, SourceLoc(), stringExprs, {},
                                       SourceLoc());
    Argument args[] = {
      Argument(SourceLoc(), C.getIdentifier("cases"), strArray),
      Argument(SourceLoc(), C.getIdentifier("string"), rawRef)
    };
    auto *argList = ArgumentList::createImplicit(C, args);
    switchArg = CallExpr::createImplicit(C, Fun, argList);
  }
  auto switchStmt =
      SwitchStmt::createImplicit(LabeledStmtInfo(), switchArg, cases, C);
  auto body = BraceStmt::create(C, SourceLoc(),
                                ASTNode(switchStmt),
                                SourceLoc());
  return { body, /*isTypeChecked=*/false };
}

static ConstructorDecl *
deriveRawRepresentable_init(DerivedConformance &derived) {
  ASTContext &C = derived.Context;

  auto enumDecl = cast<EnumDecl>(derived.Nominal);
  auto parentDC = derived.getConformanceContext();
  auto rawInterfaceType = enumDecl->getRawType();
  auto rawType = parentDC->mapTypeIntoEnvironment(rawInterfaceType);


  assert([&]() -> bool {
    return TypeChecker::conformsToKnownProtocol(
        rawType, KnownProtocolKind::Equatable);
  }());

  auto *rawDecl = new (C)
      ParamDecl(SourceLoc(), SourceLoc(),
                C.Id_rawValue, SourceLoc(), C.Id_rawValue, parentDC);
  rawDecl->setSpecifier(ParamSpecifier::Default);
  rawDecl->setInterfaceType(rawInterfaceType);
  rawDecl->setImplicit();
  auto paramList = ParameterList::createWithoutLoc(rawDecl);
  
  DeclName name(C, DeclBaseName::createConstructor(), paramList);

  auto initDecl =
      new (C) ConstructorDecl(name, SourceLoc(),
                              /*Failable=*/true, /*FailabilityLoc=*/SourceLoc(),
                              /*Async=*/false, /*AsyncLoc=*/SourceLoc(),
                              /*Throws=*/false, /*ThrowsLoc=*/SourceLoc(),
                              /*ThrownType=*/TypeLoc(), paramList,
                              /*GenericParams=*/nullptr, parentDC);

  initDecl->setImplicit();
  initDecl->setBodySynthesizer(&deriveBodyRawRepresentable_init);
  addNonIsolatedToSynthesized(enumDecl, initDecl);
  initDecl->copyFormalAccessFrom(enumDecl, /*sourceIsParentContext*/true);

  // If the containing module is not resilient, make sure clients can construct
  // an instance without function call overhead.
  maybeMarkAsInlinable(derived, initDecl);

  derived.addMembersToConformanceContext({initDecl});
  return initDecl;
}

static ValueDecl *deriveRawRepresentableViaMacros(DerivedConformance &derived,
                                                  ValueDecl *requirement) {
  auto &C = requirement->getASTContext();
  bool isStrictMemorySafety = C.LangOpts.hasFeature(
      Feature::StrictMemorySafety, /*allowMigration=*/true);

  std::string macro = "#_deriveRawRepresentable(";
  auto os = llvm::raw_string_ostream(macro);
  os << QuotedString(getNominalTypeInfoString(derived)) << ", ";

  if (requirement->getBaseName() == C.Id_rawValue) {
    os << QuotedString("varDef");
  } else {
    ASSERT(requirement->getBaseName().isConstructor());
    os << QuotedString("initializer");
  }
  os << ", " << (isStrictMemorySafety ? "true" : "false") << ")";
  os.flush();
  return deriveRequirementViaMacro(
      derived, requirement, macro,
      BuiltinDerivedConformanceMacroKind::DeriveRawRepresentable);
}

bool DerivedConformance::canDeriveRawRepresentable(DeclContext *DC,
                                                   NominalTypeDecl *type) {
  auto enumDecl = dyn_cast<EnumDecl>(type);
  if (!enumDecl)
    return false;

  Type rawType = enumDecl->getRawType();
  if (!rawType || rawType->hasError())
    return false;

  if (!computeAutomaticEnumValueKind(enumDecl))
    return false;

  rawType = DC->mapTypeIntoEnvironment(rawType);

  auto inherited = enumDecl->getInherited().getEntries();
  if (!inherited.empty() && inherited.front().wasValidated() &&
      inherited.front().isError())
    return false;

  // The raw type must be Equatable, so that we have a suitable ~= for
  // synthesized switch statements.
  if (!TypeChecker::conformsToKnownProtocol(rawType, KnownProtocolKind::Equatable))
    return false;

  auto &C = type->getASTContext();
  auto rawValueDecls = enumDecl->lookupDirect(DeclName(C.Id_RawValue));
  if (rawValueDecls.size() > 1)
    return false;

  // Check that the RawValue matches the expected raw type.
  if (!rawValueDecls.empty()) {
    if (auto alias = dyn_cast<TypeDecl>(rawValueDecls.front())) {
      auto ty = alias->getDeclaredInterfaceType();
      if (!DC->mapTypeIntoEnvironment(ty)->isEqual(rawType)) {
        return false;
      }
    }
  }

  // There must be enum elements.
  if (enumDecl->getAllElements().empty())
    return false;

  // Have the type-checker validate that:
  // - the enum elements all have the same type
  // - they all match the enum type
  for (auto elt : enumDecl->getAllElements()) {
    // We cannot synthesize raw representable conformance for an enum with
    // cases that have a payload.
    if (elt->hasAssociatedValues())
      return false;

    if (elt->isInvalid()) {
      return false;
    }
  }

  // If it meets all of those requirements, we can synthesize RawRepresentable conformance.
  return true;
}

ValueDecl *DerivedConformance::deriveRawRepresentable(ValueDecl *requirement) {

  // Check preconditions for synthesized conformance.
  if (!canDeriveRawRepresentable(cast<DeclContext>(ConformanceDecl), Nominal))
    return nullptr;

  bool viaMacros =
      Context.LangOpts.hasFeature(Feature::DeriveConformancesViaMacros);

  if (requirement->getBaseName() == Context.Id_rawValue) {
    if (!viaMacros)
      return deriveRawRepresentable_raw(*this);

    auto *witness = deriveRawRepresentableViaMacros(*this, requirement);
    if (!witness)
      return nullptr;
    maybeMarkAsInlinable(
        *this, cast<VarDecl>(witness)->getAccessor(AccessorKind::Get));
    return witness;
  }

  if (requirement->getBaseName().isConstructor()) {
    if (!viaMacros)
      return deriveRawRepresentable_init(*this);

    auto *witness = deriveRawRepresentableViaMacros(*this, requirement);
    if (!witness)
      return nullptr;
    maybeMarkAsInlinable(*this, cast<AbstractFunctionDecl>(witness));
    return witness;
  }

  Context.Diags.diagnose(requirement->getLoc(),
                         diag::broken_raw_representable_requirement);
  return nullptr;
}

Type DerivedConformance::deriveRawRepresentable(AssociatedTypeDecl *assocType) {

  // Check preconditions for synthesized conformance.
  if (!canDeriveRawRepresentable(cast<DeclContext>(ConformanceDecl), Nominal))
    return nullptr;

  if (assocType->getName() == Context.Id_RawValue) {
    return deriveRawRepresentable_Raw(*this);
  }

  Context.Diags.diagnose(assocType->getLoc(),
                         diag::broken_raw_representable_requirement);
  return nullptr;
}
