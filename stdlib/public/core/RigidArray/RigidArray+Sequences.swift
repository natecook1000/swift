//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2026 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

@available(SwiftStdlib 6.4, *)
extension RigidArray: BorrowingSequence where Element: ~Copyable {
  @available(SwiftStdlib 6.4, *)
  @_alwaysEmitIntoClient
  @_lifetime(borrow self)
  @_transparent
  public borrowing func makeBorrowingIterator() -> BorrowingSpanIterator<Element> {
    BorrowingSpanIterator(span)
  }
}

@available(SwiftStdlib 6.4, *)
extension RigidArray: MutatingSequence where Element: ~Copyable {
  @available(SwiftStdlib 6.4, *)
  @frozen
  public struct MutatingIterator: ~Copyable, ~Escapable, IteratorProtocol {
    @usableFromInline
    var array: Inout<RigidArray<Element>>

    @usableFromInline
    var i = 0

    @available(SwiftStdlib 6.4, *)
    @_alwaysEmitIntoClient
    @_lifetime(copy array)
    init(_ array: consuming Inout<RigidArray<Element>>) {
      self.array = array
    }

    @available(SwiftStdlib 6.4, *)
    @_alwaysEmitIntoClient
    @_lifetime(copy self)
    public mutating func next() -> Inout<Element>? {
      guard i < array.value.count else {
        return nil
      }

      let m = array._mutateElement(at: i)
      i &+= 1
      return m
    }
  }

  @available(SwiftStdlib 6.4, *)
  @_alwaysEmitIntoClient
  @_lifetime(&self)
  @_transparent
  public mutating func makeMutatingIterator() -> MutatingIterator {
    MutatingIterator(Inout(&self))
  }
}
