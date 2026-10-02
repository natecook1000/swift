// RUN: %target-swift-frontend -emit-sil -o /dev/null %s
// RUN: %target-swift-frontend -emit-sil -o /dev/null -enable-library-evolution %s

// Eligibility and boundary cases for the contiguous-raw-value form of the
// derived `init?(rawValue:)`. The bound it synthesizes must be representable
// in the raw type, which is only checked once the body is lowered, so these
// are `-emit-sil` tests.

// 256 cases is the largest count that fits in one byte, and 255 is the largest
// value `UInt8` can hold.
enum FullUInt8: UInt8 {
  case c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13
  case c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24, c25, c26, c27
  case c28, c29, c30, c31, c32, c33, c34, c35, c36, c37, c38, c39, c40, c41
  case c42, c43, c44, c45, c46, c47, c48, c49, c50, c51, c52, c53, c54, c55
  case c56, c57, c58, c59, c60, c61, c62, c63, c64, c65, c66, c67, c68, c69
  case c70, c71, c72, c73, c74, c75, c76, c77, c78, c79, c80, c81, c82, c83
  case c84, c85, c86, c87, c88, c89, c90, c91, c92, c93, c94, c95, c96, c97
  case c98, c99, c100, c101, c102, c103, c104, c105, c106, c107, c108, c109, c110, c111
  case c112, c113, c114, c115, c116, c117, c118, c119, c120, c121, c122, c123, c124, c125
  case c126, c127, c128, c129, c130, c131, c132, c133, c134, c135, c136, c137, c138, c139
  case c140, c141, c142, c143, c144, c145, c146, c147, c148, c149, c150, c151, c152, c153
  case c154, c155, c156, c157, c158, c159, c160, c161, c162, c163, c164, c165, c166, c167
  case c168, c169, c170, c171, c172, c173, c174, c175, c176, c177, c178, c179, c180, c181
  case c182, c183, c184, c185, c186, c187, c188, c189, c190, c191, c192, c193, c194, c195
  case c196, c197, c198, c199, c200, c201, c202, c203, c204, c205, c206, c207, c208, c209
  case c210, c211, c212, c213, c214, c215, c216, c217, c218, c219, c220, c221, c222, c223
  case c224, c225, c226, c227, c228, c229, c230, c231, c232, c233, c234, c235, c236, c237
  case c238, c239, c240, c241, c242, c243, c244, c245, c246, c247, c248, c249, c250, c251
  case c252, c253, c254, c255
}

func useFullUInt8(_ raw: UInt8) -> FullUInt8? { FullUInt8(rawValue: raw) }

// Likewise for a signed raw type, where the last raw value is Int8.max.
enum FullInt8: Int8 {
  case c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13
  case c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24, c25, c26, c27
  case c28, c29, c30, c31, c32, c33, c34, c35, c36, c37, c38, c39, c40, c41
  case c42, c43, c44, c45, c46, c47, c48, c49, c50, c51, c52, c53, c54, c55
  case c56, c57, c58, c59, c60, c61, c62, c63, c64, c65, c66, c67, c68, c69
  case c70, c71, c72, c73, c74, c75, c76, c77, c78, c79, c80, c81, c82, c83
  case c84, c85, c86, c87, c88, c89, c90, c91, c92, c93, c94, c95, c96, c97
  case c98, c99, c100, c101, c102, c103, c104, c105, c106, c107, c108, c109, c110, c111
  case c112, c113, c114, c115, c116, c117, c118, c119, c120, c121, c122, c123, c124, c125
  case c126, c127
}

func useFullInt8(_ raw: Int8) -> FullInt8? { FullInt8(rawValue: raw) }

// Just past one byte, so the tag no longer fits the byte the fast path uses.
enum JustOverAByte: UInt16 {
  case c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13
  case c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24, c25, c26, c27
  case c28, c29, c30, c31, c32, c33, c34, c35, c36, c37, c38, c39, c40, c41
  case c42, c43, c44, c45, c46, c47, c48, c49, c50, c51, c52, c53, c54, c55
  case c56, c57, c58, c59, c60, c61, c62, c63, c64, c65, c66, c67, c68, c69
  case c70, c71, c72, c73, c74, c75, c76, c77, c78, c79, c80, c81, c82, c83
  case c84, c85, c86, c87, c88, c89, c90, c91, c92, c93, c94, c95, c96, c97
  case c98, c99, c100, c101, c102, c103, c104, c105, c106, c107, c108, c109, c110, c111
  case c112, c113, c114, c115, c116, c117, c118, c119, c120, c121, c122, c123, c124, c125
  case c126, c127, c128, c129, c130, c131, c132, c133, c134, c135, c136, c137, c138, c139
  case c140, c141, c142, c143, c144, c145, c146, c147, c148, c149, c150, c151, c152, c153
  case c154, c155, c156, c157, c158, c159, c160, c161, c162, c163, c164, c165, c166, c167
  case c168, c169, c170, c171, c172, c173, c174, c175, c176, c177, c178, c179, c180, c181
  case c182, c183, c184, c185, c186, c187, c188, c189, c190, c191, c192, c193, c194, c195
  case c196, c197, c198, c199, c200, c201, c202, c203, c204, c205, c206, c207, c208, c209
  case c210, c211, c212, c213, c214, c215, c216, c217, c218, c219, c220, c221, c222, c223
  case c224, c225, c226, c227, c228, c229, c230, c231, c232, c233, c234, c235, c236, c237
  case c238, c239, c240, c241, c242, c243, c244, c245, c246, c247, c248, c249, c250, c251
  case c252, c253, c254, c255, c256
}

func useJustOverAByte(_ raw: UInt16) -> JustOverAByte? { JustOverAByte(rawValue: raw) }

// A single-case enum is zero-sized, so it has no tag byte to convert.
enum Single: Int {
  case only
}

func useSingle(_ raw: Int) -> Single? { Single(rawValue: raw) }

// An unavailable case still consumes its tag index.
enum WithUnavailable: Int {
  case a = 0
  @available(*, unavailable)
  case b = 1
  case c = 2
}

func useWithUnavailable(_ raw: Int) -> WithUnavailable? { WithUnavailable(rawValue: raw) }

// A conditionally available case needs a runtime query.
enum WithConditionalAvailability: Int {
  case a = 0
  @available(macOS 99, iOS 99, watchOS 99, tvOS 99, visionOS 99, *)
  case b = 1
}

func useWithConditionalAvailability(_ raw: Int) -> WithConditionalAvailability? {
  WithConditionalAvailability(rawValue: raw)
}

// Non-integer raw types have integer-literal raw values, but no relation to
// the tag.
enum DoubleRaw: Double {
  case a = 0, b = 1, c = 2
}

func useDoubleRaw(_ raw: Double) -> DoubleRaw? { DoubleRaw(rawValue: raw) }

enum StringRaw: String {
  case a, b, c
}

func useStringRaw(_ raw: String) -> StringRaw? { StringRaw(rawValue: raw) }

// Contiguous but not zero-based, out of order, and including negatives.
enum OneBased: Int {
  case a = 1, b = 2, c = 3
}

func useOneBased(_ raw: Int) -> OneBased? { OneBased(rawValue: raw) }

enum Shuffled: Int {
  case a = 2, b = 0, c = 1
}

func useShuffled(_ raw: Int) -> Shuffled? { Shuffled(rawValue: raw) }

enum Negative: Int {
  case a = -1, b = 0, c = 1
}

func useNegative(_ raw: Int) -> Negative? { Negative(rawValue: raw) }

// A user-written initializer suppresses derivation.
enum UserWritten: Int {
  case a, b, c

  init?(rawValue: Int) {
    guard rawValue >= 0, rawValue < 3 else { return nil }
    self = [.a, .b, .c][rawValue]
  }
}

func useUserWritten(_ raw: Int) -> UserWritten? { UserWritten(rawValue: raw) }
