// RUN: %target-run-simple-swift
// RUN: %target-run-simple-swift(-O)
// RUN: %target-run-simple-swift(-O -enable-library-evolution)
// REQUIRES: executable_test

// The derived `init?(rawValue:)` for an enum whose raw values are the
// contiguous case indices is synthesized as a range check and a bit cast. It
// must stay fully checked: every in-range value maps to its own case, and
// every out-of-range value returns nil.

import StdlibUnittest

let suite = TestSuite("EnumRawValueInit")

enum Thirty: UInt64 {
  case c0, c1, c2, c3, c4, c5, c6, c7, c8, c9
  case c10, c11, c12, c13, c14, c15, c16, c17, c18, c19
  case c20, c21, c22, c23, c24, c25, c26, c27, c28, c29
}

suite.test("unsigned/inRange") {
  for i in 0 ..< 30 {
    let value = Thirty(rawValue: UInt64(i))
    expectNotNil(value)
    expectEqual(UInt64(i), value!.rawValue)
  }
}

suite.test("unsigned/outOfRange") {
  for raw: UInt64 in [30, 31, 100, 255, 256, 1 << 32, UInt64.max] {
    expectNil(Thirty(rawValue: raw))
  }
}

enum SignedTen: Int {
  case s0, s1, s2, s3, s4, s5, s6, s7, s8, s9
}

suite.test("signed/inRange") {
  for i in 0 ..< 10 {
    let value = SignedTen(rawValue: i)
    expectNotNil(value)
    expectEqual(i, value!.rawValue)
  }
}

suite.test("signed/outOfRange") {
  // An upper bound alone would admit the negative values.
  for raw in [-1, -2, -128, -129, Int.min, 10, 11, 128, 255, 256, Int.max] {
    expectNil(SignedTen(rawValue: raw))
  }
}

// Narrow raw types.
enum NarrowUnsigned: UInt8 {
  case c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15
}

suite.test("narrowUnsigned") {
  for i in 0 ..< 16 {
    expectEqual(UInt8(i), NarrowUnsigned(rawValue: UInt8(i))!.rawValue)
  }
  for raw: UInt8 in [16, 17, 100, 255] {
    expectNil(NarrowUnsigned(rawValue: raw))
  }
}

enum NarrowSigned: Int8 {
  case c0, c1, c2, c3
}

suite.test("narrowSigned") {
  for i: Int8 in 0 ..< 4 {
    expectEqual(i, NarrowSigned(rawValue: i)!.rawValue)
  }
  for raw: Int8 in [-1, -128, 4, 5, 127] {
    expectNil(NarrowSigned(rawValue: raw))
  }
}

// Ineligible shapes keep working unchanged.

enum Sparse: UInt64 {
  case a = 1, b = 100, c = 10000
}

suite.test("sparse") {
  expectEqual(.a, Sparse(rawValue: 1))
  expectEqual(.b, Sparse(rawValue: 100))
  expectEqual(.c, Sparse(rawValue: 10000))
  for raw: UInt64 in [0, 2, 99, 101, 9999, 10001, UInt64.max] {
    expectNil(Sparse(rawValue: raw))
  }
}

enum OneBased: Int {
  case a = 1, b = 2, c = 3
}

suite.test("oneBased") {
  expectNil(OneBased(rawValue: 0))
  expectEqual(.a, OneBased(rawValue: 1))
  expectEqual(.c, OneBased(rawValue: 3))
  expectNil(OneBased(rawValue: 4))
}

enum Shuffled: Int {
  case a = 2, b = 0, c = 1
}

suite.test("declaredOutOfOrder") {
  // The tag is the declaration index, so a bit cast would map 0 to `a`.
  expectEqual(.b, Shuffled(rawValue: 0))
  expectEqual(.c, Shuffled(rawValue: 1))
  expectEqual(.a, Shuffled(rawValue: 2))
  expectNil(Shuffled(rawValue: 3))
}

enum Negative: Int {
  case a = -1, b = 0, c = 1
}

suite.test("negativeRawValues") {
  expectEqual(.a, Negative(rawValue: -1))
  expectEqual(.b, Negative(rawValue: 0))
  expectEqual(.c, Negative(rawValue: 1))
  expectNil(Negative(rawValue: 2))
  expectNil(Negative(rawValue: -2))
}

enum Single: Int {
  case only
}

suite.test("singleCase") {
  expectEqual(.only, Single(rawValue: 0))
  expectNil(Single(rawValue: 1))
  expectNil(Single(rawValue: -1))
}

// An unavailable case still consumes its tag index.
enum WithUnavailable: Int {
  case a = 0
  @available(*, unavailable)
  case b = 1
  case c = 2
}

suite.test("unavailableCase") {
  expectEqual(.a, WithUnavailable(rawValue: 0))
  expectNil(WithUnavailable(rawValue: 1))
  expectEqual(.c, WithUnavailable(rawValue: 2))
  expectNil(WithUnavailable(rawValue: 3))
}

enum DoubleRaw: Double {
  case a = 0, b = 1, c = 2
}

suite.test("floatingPointRawValues") {
  expectEqual(.a, DoubleRaw(rawValue: 0))
  expectEqual(.c, DoubleRaw(rawValue: 2))
  expectNil(DoubleRaw(rawValue: 0.5))
  expectNil(DoubleRaw(rawValue: 3))
}

runAllTests()
