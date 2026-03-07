//===--- NewSequences.swift --------------------------------------------------===//
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

// RUN: %target-run-stdlib-swift(-enable-experimental-feature SuppressedAssociatedTypesWithDefaults -enable-experimental-feature BorrowInout -enable-experimental-feature BorrowingSequence -enable-experimental-feature Lifetimes -Xfrontend -disable-availability-checking -enable-experimental-feature AddressableParameters -enable-experimental-feature AddressableTypes)

// REQUIRES: executable_test
// REQUIRES: swift_feature_SuppressedAssociatedTypesWithDefaults
// REQUIRES: swift_feature_BorrowingSequence

import StdlibUnittest

var suite = TestSuite("NewSequences")
defer { runAllTests() }

struct NCInt: ~Copyable {
  var x = 0

  deinit {
    print("Deinitializing NCInt: \(x)")
  }
}

func makeUniqueArray() -> UniqueArray<NCInt> {
  var ua = UniqueArray<NCInt>()
  for i in 1...5 {
    ua.append(NCInt(x: i))
  }
  return ua
}


extension BorrowingSequence where Self: ~Escapable & ~Copyable, BorrowedElement: ~Copyable & ~Escapable {
  var borrowing: BorrowingIterator {
    @_lifetime(borrow self)
    borrowing get {
      makeBorrowingIterator()
    }
  }
}

extension MutatingSequence where Self: ~Escapable & ~Copyable, MutableElement: ~Copyable & ~Escapable {
  var mutating: MutatingIterator {
    @_lifetime(&self)
    mutating get {
      makeMutatingIterator()
    }
  }
}

// MARK: - contains(where:)

extension IteratorProtocol where Self: ~Copyable & ~Escapable, Element: ~Copyable & ~Escapable {
  consuming func contains(where predicate: (borrowing Element) -> Bool) -> Bool {
    while let el = next() {
      if predicate(el) { return true }
    }
    return false
  }
  
  consuming func contains<T>(where predicate: (borrowing T) -> Bool) -> Bool
    where Element == Borrow<T>
  {
    while let el = next() {
      if predicate(el.value) { return true }
    }
    return false
  }
}

//protocol _BorrowingSequence<Element: ~Copyable> {
//  associatedtype BorrowingIterator: _BorrowingIteratorProtocol<Element>
//  
//  @_lifetime(borrow self)
//  func makeBorrowingIterator() -> BorrowingIterator<Element>
//}
//
//protocol _BorrowingIteratorProtocol<Element: ~Copyable>: ... {
//  @_lifetime(copy self)
//  mutating func next() -> Borrow<Element>?
//}
//
//struct Tuple<T: ~Copyable, U: ~Copyable>: ~Copyable & ~Escapable {
//  let a: T
//  let b: U
//}

suite.test("contains(where:)")
  .require(.stdlib_6_4).code {
    guard #available(SwiftStdlib 6.4, *) else {
      return
    }
    
    do {
      let uniqueArray = makeUniqueArray()
      
      // uniqueArray.borrowing -> BorrowingSpanIterator<NCInt>
      // - element type of the iterator is `Borrow<NCInt>`
      // - func contains(where predicate: (borrowing Borrow<NCInt>) -> Bool) -> Bool
      
      let greaterThan2 = uniqueArray
        .borrowing
        .contains(where: { $0.value.x > 2 })
      let greaterThan8 = uniqueArray.borrowing
        .contains(where: { $0.value.x > 8 })
      
      expectTrue(greaterThan2)
      expectFalse(greaterThan8)
      
      // MARK: Exploration of `borrowing T` vs `Borrow<T>`
      
      func foo(_ x: borrowing NCInt) {}
      
      var borrowingIterator = uniqueArray.borrowing
      while let borrowedElement = borrowingIterator.next() {
        // I can call a method that takes a `borrowing NCInt` parameter
        foo(borrowedElement.value)

        // I should be able to write:
        //   let x: borrowing NCInt = borrowedElement.value
        //
        // or better yet:
        //   the binding above should be to `borrowing NCInt` instead of `Borrow<NCInt>`
        //
        // or even better:
        //   `borrowing NCInt` and `Borrow<NCInt>` should be equivalent
      }
    }
    
    do {
      let inline: InlineArray = [1, 2, 3, 4, 5]

      let greaterThan2 = inline.borrowing
        .contains(where: { $0.value > 2 })
      let greaterThan8 = inline.borrowing
        .contains(where: { $0.value > 8 })
      
      expectTrue(greaterThan2)
      expectFalse(greaterThan8)
    }
  }

// MARK: - first(where:)

extension IteratorProtocol where Self: ~Copyable & ~Escapable, Element: ~Copyable & ~Escapable {
  @_lifetime(copy self)
  consuming func first(where predicate: (borrowing Element) -> Bool) -> Element? {
    while let el = next() {
      if predicate(el) { return el }
    }
    return nil
  }
}

suite.test("first(where:)")
  .require(.stdlib_6_4).code {
    guard #available(SwiftStdlib 6.4, *) else {
      return
    }
    
    do {
      let uniqueArray = makeUniqueArray()
      
      let greaterThan2 = uniqueArray.borrowing
        .first(where: { $0.value.x > 2 })
      expectEqual(greaterThan2?.value.x, 3)
      
      let greaterThan8 = uniqueArray.borrowing
        .first(where: { $0.value.x > 8 })
      expectNil(greaterThan8)
    }
    
    do {
      let inline: InlineArray = [1, 2, 3, 4, 5]

      let greaterThan2 = inline.borrowing
        .first(where: { $0.value > 2 })
      expectEqual(greaterThan2?.value, 3)
      
      let greaterThan8 = inline.borrowing
        .first(where: { $0.value > 8 })
      expectNil(greaterThan8)
    }
  }

// MARK: - min(by:)

extension IteratorProtocol where Self: ~Copyable & ~Escapable, Element: ~Copyable & ~Escapable {
  @_lifetime(copy self)
  consuming func min(by areInIncreasingOrder: (borrowing Element, borrowing Element) -> Bool) -> Element? {
    guard var result = next() else { return nil }
    while let el = next() {
      if areInIncreasingOrder(el, result) {
        result = el
      }
    }
    return result
  }
}

suite.test("min(by:)")
  .require(.stdlib_6_4).code {
    guard #available(SwiftStdlib 6.4, *) else {
      return
    }
    
    do {
      let uniqueArray = makeUniqueArray()
      
      let minimum = uniqueArray.borrowing
        .min(by: { $0.value.x < $1.value.x })
      expectEqual(minimum?.value.x, 1)
    }
    
    do {
      let inline: InlineArray = [1, 2, 3, 4, 5]

      let minimum = inline.borrowing
        .min(by: { $0.value < $1.value })
      expectEqual(minimum?.value, 1)
    }
  }

// MARK: - reduce

extension IteratorProtocol where Self: ~Copyable & ~Escapable, Element: ~Copyable & ~Escapable {
//  @_lifetime(copy self)
  consuming func reduce<T: ~Copyable>(
    _ initial: consuming T,
    _ nextPartialResult: (consuming T, borrowing Element) -> T
  ) -> T {
    var result = initial
    while let el = next() {
      result = nextPartialResult(result, el)
    }
    return result
  }
}

suite.test("reduce")
  .require(.stdlib_6_4).code {
    guard #available(SwiftStdlib 6.4, *) else {
      return
    }
    
    do {
      let uniqueArray = makeUniqueArray()
      
      let sum = uniqueArray.borrowing
        .reduce(0, { $0 + $1.value.x })
      expectEqual(sum, 15)
    }
    
    do {
      let inline: InlineArray = [1, 2, 3, 4, 5]

      let sum = inline.borrowing
        .reduce(0, { $0 + $1.value })
      expectEqual(sum, 15)
    }
  }

// MARK: - lazy map

suite.test("map")
  .require(.stdlib_6_4).code {
    guard #available(SwiftStdlib 6.4, *) else {
      return
    }
    
    let uniqueArray = makeUniqueArray()
    let mapped = uniqueArray.borrowing.map {
      $0.value.x + 2
    }
    let sum = mapped.reduce(0, +)
    expectEqual(sum, 25)

    var mutableArray = makeUniqueArray()
//    let mapped = mutableArray.mutating.map {
//      $0.value.x += 1
//      return $0.value.x + 1
//    }
//    let sum = mapped.reduce(0, +)
//    expectEqual(sum, 25)

  }






suite.test("test")
.require(.stdlib_6_4).code {
 guard #available(SwiftStdlib 6.4, *) else {
   expectTrue(false)
   return
 }
 
  var ua = makeUniqueArray()

  var iter = ua.makeBorrowingIterator()
  while let ncInt = iter.next() {
    print("NCInt: \(ncInt.value.x)")
  }

  var mutIter = ua.makeMutatingIterator()
  while var ncInt = mutIter.next() {
    ncInt.value.x += 1
  }

  var mapIter = ua.makeBorrowingIterator().map {
    $0.value.x
  }
  while let int = mapIter.next() {
    print("x in NCInt: \(int)")
  }

  var mapMutIter = ua.makeMutatingIterator().map {
    var tmp = $0
    let prev = tmp.value.x
    tmp.value.x += 1
    return prev
  }
  while let int = mapMutIter.next() {
    print("Previous NCInt: \(int)")
  }
  
  var a = [1, 2, 3]
  var mapArrayIter: LazyMapIter = a.makeIterator().map { $0 + 1 }
  while let n = mapArrayIter.next() {
    print(n)
  }
  
  let ua2 = makeUniqueArray()
  
  expectTrue(ua2.borrowing.contains(where: { $0.value.x > 2 }))
  
  let inline: InlineArray = [1, 2, 3, 4, 5]
  var inlineIter: LazyMapIter = inline.borrowing.map { $0.value + 1 }
  expectTrue(inlineIter.next() == 2)
  expectTrue(inlineIter.next() == 3)
  expectTrue(inlineIter.next() == 4)
  expectTrue(inlineIter.next() == 5)
  expectTrue(inlineIter.next() == 6)
}

//
//
//protocol BorrowingSequence2<Element>: ~Copyable, ~Escapable where Element: ~Copyable {
//  associatedtype BorrowingIterator: IteratorProtocol  where BorrowingIterator.Element == Borrow<Element>
//}
//
//@available(SwiftStdlib 6.4, *)
//public protocol BorrowingSequence2<Element>: ~Copyable & ~Escapable {
//  associatedtype Element: ~Copyable & ~Escapable
//  associatedtype BorrowingIterator: BorrowingIteratorProtocol2<Element> & ~Copyable & ~Escapable
//
//  @_lifetime(borrow self)
//  borrowing func makeBorrowingIterator() -> BorrowingIterator
//}
//
//protocol BorrowingIteratorProtocol2: IteratorProtocol where Element: ~Copyable {
//  
//}
