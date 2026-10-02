// RUN: %target-swift-frontend -parse-as-library -primary-file %s -O -module-name=test -emit-sil | %FileCheck %s
// RUN: %target-swift-frontend -parse-as-library -primary-file %s -O -module-name=test -emit-sil -enable-library-evolution | %FileCheck %s

// The derived `init?(rawValue:)` of an enum whose raw values are the
// contiguous case indices lowers to a range check and a bit cast, rather than
// to a comparison per case, and so is inlined at any number of cases.
//
// `8rawValue` appears only in mangled names, so CHECK-NOT on it asserts that
// no call to the initializer survives.

public enum Opcode: UInt64 {
  case c0, c1, c2, c3, c4, c5, c6, c7, c8, c9
  case c10, c11, c12, c13, c14, c15, c16, c17, c18, c19
  case c20, c21, c22, c23, c24, c25, c26, c27, c28, c29
}

// CHECK-LABEL: sil [noinline] @$s4test14decodeOptionalyAA6OpcodeOSgs6UInt64VF :
// CHECK-NOT:     8rawValue
// CHECK-NOT:     builtin "cmp_eq_Int64"
// CHECK:         integer_literal $Builtin.Int64, 29
// CHECK:         builtin "cmp_{{[us]}}{{(ge|gt|le|lt)}}_Int64"
// CHECK:         unchecked_trivial_bit_cast {{.*}} to $Opcode
// CHECK-NOT:     8rawValue
// CHECK-NOT:     builtin "cmp_eq_Int64"
// CHECK:       } // end sil function '$s4test14decodeOptionalyAA6OpcodeOSgs6UInt64VF'
@inline(never)
public func decodeOptional(_ raw: UInt64) -> Opcode? {
  Opcode(rawValue: raw & 0xFF)
}

// The shape a dispatch loop uses.

// CHECK-LABEL: sil [noinline] @$s4test15decodeUnwrappedyAA6OpcodeOs6UInt64VF :
// CHECK-NOT:     8rawValue
// CHECK-NOT:     builtin "cmp_eq_Int64"
// CHECK:         builtin "cmp_{{[us]}}{{(ge|gt|le|lt)}}_Int64"
// CHECK-NOT:     8rawValue
// CHECK-NOT:     builtin "cmp_eq_Int64"
// CHECK:       } // end sil function '$s4test15decodeUnwrappedyAA6OpcodeOs6UInt64VF'
@inline(never)
public func decodeUnwrapped(_ raw: UInt64) -> Opcode {
  Opcode(rawValue: raw & 0xFF).unsafelyUnwrapped
}

// A call-free decode leaves the enclosing loop open to further optimization.

// CHECK-LABEL: sil [noinline] @$s4test8dispatchySiSays6UInt64VGF :
// CHECK-NOT:     8rawValue
// CHECK:       } // end sil function '$s4test8dispatchySiSays6UInt64VGF'
@inline(never)
public func dispatch(_ program: [UInt64]) -> Int {
  var total = 0
  for word in program {
    switch Opcode(rawValue: (word >> 56) & 0xFF).unsafelyUnwrapped {
    case .c0: total &+= 1
    case .c1: total &+= 2
    default:  total &+= 3
    }
  }
  return total
}

// A signed raw type also needs a lower bound.

public enum Signed: Int {
  case s0, s1, s2, s3, s4, s5, s6, s7, s8, s9
}

// CHECK-LABEL: sil [noinline] @$s4test12decodeSignedyAA0C0OSgSiF :
// CHECK-NOT:     8rawValue
// CHECK-NOT:     builtin "cmp_eq_Int64"
// CHECK:         builtin "cmp_s{{(ge|gt|le|lt)}}_Int64"
// CHECK:         builtin "cmp_s{{(ge|gt|le|lt)}}_Int64"
// CHECK:         unchecked_trivial_bit_cast {{.*}} to $Signed
// CHECK-NOT:     8rawValue
// CHECK-NOT:     builtin "cmp_eq_Int64"
// CHECK:       } // end sil function '$s4test12decodeSignedyAA0C0OSgSiF'
@inline(never)
public func decodeSigned(_ raw: Int) -> Signed? {
  Signed(rawValue: raw)
}

// Enums whose tag is not their raw value must still use the comparison chain.

// Raw values are not contiguous.
public enum Sparse: UInt64 {
  case a = 1
  case b = 100
  case c = 10000
}

// CHECK-LABEL: sil [noinline] @$s4test12decodeSparseyAA0C0OSgs6UInt64VF :
// CHECK:         builtin "cmp_eq_Int64"
// CHECK-NOT:     unchecked_trivial_bit_cast
// CHECK:       } // end sil function '$s4test12decodeSparseyAA0C0OSgs6UInt64VF'
@inline(never)
public func decodeSparse(_ raw: UInt64) -> Sparse? {
  Sparse(rawValue: raw)
}

// Not zero-based: the tag is the raw value minus one.
public enum OneBased: Int {
  case a = 1, b = 2, c = 3, d = 4
}

// CHECK-LABEL: sil [noinline] @$s4test14decodeOneBasedyAA0cD0OSgSiF :
// CHECK:         builtin "cmp_eq_Int64"
// CHECK-NOT:     unchecked_trivial_bit_cast
// CHECK:       } // end sil function '$s4test14decodeOneBasedyAA0cD0OSgSiF'
@inline(never)
public func decodeOneBased(_ raw: Int) -> OneBased? {
  OneBased(rawValue: raw)
}

// Declared out of ascending order, so `b` has tag 1 rather than tag 0.
public enum Shuffled: Int {
  case a = 2, b = 0, c = 1
}

// CHECK-LABEL: sil [noinline] @$s4test14decodeShuffledyAA0C0OSgSiF :
// CHECK:         builtin "cmp_eq_Int64"
// CHECK-NOT:     unchecked_trivial_bit_cast
// CHECK:       } // end sil function '$s4test14decodeShuffledyAA0C0OSgSiF'
@inline(never)
public func decodeShuffled(_ raw: Int) -> Shuffled? {
  Shuffled(rawValue: raw)
}

// A String raw type keeps its own fast path, `_findStringSwitchCase`.
public enum Str: String {
  case a, b, c
}

// CHECK-LABEL: sil [noinline] @$s4test9decodeStryAA0C0OSgSSF :
// CHECK-NOT:     unchecked_trivial_bit_cast
// CHECK:       } // end sil function '$s4test9decodeStryAA0C0OSgSSF'
@inline(never)
public func decodeStr(_ raw: String) -> Str? {
  Str(rawValue: raw)
}
