// RUN: %target-run-simple-swift
// REQUIRES: executable_test

import StdlibUnittest

var GuardCatchTests = TestSuite("GuardCatch")

enum NetErr: Error, Equatable { case timeout, refused }
enum Other: Error { case err }
enum FileErr: Error, Equatable { case notFound }

enum Sentinels {
  static let mayThrow          = 1000
  static let typedThrow        = 1001
  static let sometimesNil      = 1002
  static let optInt            = 1003
  static let typedThrow2       = 1004
  static let fileThrow         = 1005
  static let untypedThrow      = 1006
  static let typedSometimesNil = 1007
}

// Helper functions varied across the cases below.
var voidShouldThrow = false
func voidThrowing() throws {
  if voidShouldThrow { throw NetErr.timeout }
}

func mayThrow(_ n: Int) throws -> Int {
  if n < 0 || n == Sentinels.mayThrow { throw Other.err }
  return n
}

func typedThrow(_ n: Int) throws(NetErr) -> Int {
  if n < 0 || n == Sentinels.typedThrow { throw NetErr.refused }
  return n
}

func sometimesNil(_ n: Int) throws -> Int? {
  if n < 0 || n == Sentinels.sometimesNil { throw NetErr.timeout }
  return n == 0 ? nil : n
}

func optInt(_ n: Int) -> Int? {
  if n < 0 || n == Sentinels.optInt { return nil }
  return n
}

// Throws NetErr but with a different case than typedThrow, for same-type tests.
func typedThrow2(_ n: Int) throws(NetErr) -> Int {
  if n < 0 || n == Sentinels.typedThrow2 { throw NetErr.timeout }
  return n
}

// Throws a different typed error, for mixed-type tests.
func fileThrow(_ n: Int) throws(FileErr) -> Int {
  if n < 0 || n == Sentinels.fileThrow { throw FileErr.notFound }
  return n
}

// Untyped throw, for typed+untyped erasure tests.
func untypedThrow(_ n: Int) throws -> Int {
  if n < 0 || n == Sentinels.untypedThrow { throw NetErr.refused }
  return n
}

// Typed optional throw, for re-throw shape-(2) tests.
func typedSometimesNil(_ n: Int) throws(NetErr) -> Int? {
  if n < 0 || n == Sentinels.typedSometimesNil { throw NetErr.refused }
  return n == 0 ? nil : n
}

// MARK: catch-only with non-Optional throwing binding.

func catchOnly(_ x: Int) -> Int {
  guard let n = try mayThrow(x) catch { return -1 }
  return n
}

GuardCatchTests.test("catchOnly returns the bound value when no throw") {
  expectEqual(7, catchOnly(7))
}
GuardCatchTests.test("catchOnly routes to catch on throw") {
  expectEqual(-1, catchOnly(-5))
}

// MARK: typed error catch-only with non-Optional throwing binding.

func catchOnlyTypedError(_ x: Int) -> Int {
  guard let n = try typedThrow(x) catch {
    return error == .refused ? -1 : -2
  }
  return n
}

GuardCatchTests.test("catchOnly w/ typed error returns the bound value") {
  expectEqual(7, catchOnlyTypedError(7))
}
GuardCatchTests.test("catchOnly w/ typed error routes to catch on throw") {
  expectEqual(-1, catchOnlyTypedError(-5))
}

// MARK: catch-only with bare void-throwing element.

func catchOnlyVoid() -> String {
  guard try voidThrowing() catch { return "err" }
  return "ok"
}

GuardCatchTests.test("catchOnly bare void: success path") {
  voidShouldThrow = false
  expectEqual("ok", catchOnlyVoid())
}
GuardCatchTests.test("catchOnly bare void: throw path") {
  voidShouldThrow = true
  expectEqual("err", catchOnlyVoid())
  voidShouldThrow = false
}

// MARK: guard with else + catches with both paths reachable.

func elseAndCatch(_ x: Int) -> String {
  guard let v = try sometimesNil(x) else {
    return "got nil"
  } catch {
    return "caught"
  }
  return "v=\(v)"
}

GuardCatchTests.test("shape2 returns bound value on success") {
  expectEqual("v=5", elseAndCatch(5))
}
GuardCatchTests.test("shape2 routes nil to else") {
  expectEqual("got nil", elseAndCatch(0))
}
GuardCatchTests.test("shape2 routes throw to catch") {
  expectEqual("caught", elseAndCatch(-1))
}

// MARK: mixed Optional binding + void-throwing element.

func mixed(_ n: Int?) -> String {
  guard let val = n,
        try voidThrowing()
  else {
    return "nil"
  } catch {
    return "caught"
  }
  return "val=\(val)"
}

GuardCatchTests.test("mixed: optional nil routes to else") {
  voidShouldThrow = false
  expectEqual("nil", mixed(nil))
}
GuardCatchTests.test("mixed: void throw routes to catch") {
  voidShouldThrow = true
  expectEqual("caught", mixed(42))
  voidShouldThrow = false
}
GuardCatchTests.test("mixed: success") {
  voidShouldThrow = false
  expectEqual("val=42", mixed(42))
}

// MARK: typed-pattern catch dispatches like do-catch.

func typedDispatch(_ x: Int) -> String {
  guard let v = try sometimesNil(x) else {
    return "nil"
  } catch NetErr.timeout {
    return "timeout"
  } catch {
    return "other"
  }
  return "v=\(v)"
}

GuardCatchTests.test("typed dispatch: fallthrough") {
  expectEqual("v=1", typedDispatch(1))
}

GuardCatchTests.test("typed dispatch: nil case") {
  expectEqual("nil", typedDispatch(0))
}

GuardCatchTests.test("typed dispatch: matched case") {
  expectEqual("timeout", typedDispatch(-1))
}

// MARK: complex guard statement with permutation of different conditions

func permutations(_ n: Int) -> String {
  guard
    let num1 = try typedThrow(n),         // typed throwing function with non-optional result
    let num2 = optInt(num1),              // function with optional result
    let num3 = try mayThrow(num2),        // throwing function with non-optional result
    let num4 = try sometimesNil(num3),    // throwing function with optional result
    num4.isMultiple(of: 2),               // Boolean function
    String(n).count < 3                   // Boolean expression
  else {
    return "else"
  } catch _ as NetErr {
    return "neterr"
  } catch {
    return "error"
  }
  return String(n)
}

GuardCatchTests.test("permutations: success path") {
  expectEqual("2", permutations(2))
}
GuardCatchTests.test("permutations: typed throw routes to typed catch") {
  expectEqual("neterr", permutations(Sentinels.typedThrow))
}
GuardCatchTests.test("permutations: other throw routes to untyped catch") {
  expectEqual("error", permutations(Sentinels.mayThrow))
}
GuardCatchTests.test("permutations: nil from sometimesNil routes to else") {
  expectEqual("else", permutations(0))
}
GuardCatchTests.test("permutations: false Boolean function routes to else") {
  expectEqual("else", permutations(3))
}
GuardCatchTests.test("permutations: false Boolean expression routes to else") {
  expectEqual("else", permutations(222))
}

// MARK: typed-error inference — two calls, same typed error stays typed.
//
// The exhaustive switch on `error` is valid only if its type is NetErr;
// it would fail to compile if the type were erased to `any Error`.

func twoSameTyped(_ a: Int, _ b: Int) -> String {
  guard let x = try typedThrow(a),
        let y = try typedThrow2(b)
  catch {
    switch error {
    case .refused: return "refused"
    case .timeout: return "timeout"
    }
  }
  return "ok:\(x + y)"
}

GuardCatchTests.test("twoSameTyped: success") {
  expectEqual("ok:11", twoSameTyped(1, 10))
}
GuardCatchTests.test("twoSameTyped: first call throws .refused") {
  expectEqual("refused", twoSameTyped(-1, 10))
}
GuardCatchTests.test("twoSameTyped: second call throws .timeout") {
  expectEqual("timeout", twoSameTyped(1, -1))
}

// MARK: typed-error inference — two different typed errors erase to any Error.
//
// When the conditions throw two distinct typed errors, the inferred type is
// `any Error` and the catch body must downcast to inspect the concrete error.

func twoDifferentTyped(_ a: Int, _ b: Int) -> String {
  guard let x = try typedThrow(a),
        let y = try fileThrow(b)
  catch {
    if let e = error as? NetErr { return "net:\(e)" }
    if let e = error as? FileErr { return "file:\(e)" }
    return "other"
  }
  return "ok:\(x + y)"
}

GuardCatchTests.test("twoDifferentTyped: success") {
  expectEqual("ok:6", twoDifferentTyped(1, 5))
}
GuardCatchTests.test("twoDifferentTyped: first call throws NetErr") {
  expectEqual("net:refused", twoDifferentTyped(-1, 5))
}
GuardCatchTests.test("twoDifferentTyped: second call throws FileErr") {
  expectEqual("file:notFound", twoDifferentTyped(1, -1))
}

// MARK: typed-error inference — typed + untyped throw erases to any Error.
//
// Mixing a typed throw with an untyped one erases the error type even if
// the untyped function happens to throw the same concrete type at runtime.

func typedAndUntyped(_ a: Int, _ b: Int) -> String {
  guard let x = try typedThrow(a),
        let y = try untypedThrow(b)
  catch {
    if let e = error as? NetErr { return "net:\(e)" }
    return "other"
  }
  return "ok:\(x + y)"
}

GuardCatchTests.test("typedAndUntyped: success") {
  expectEqual("ok:11", typedAndUntyped(1, 10))
}
GuardCatchTests.test("typedAndUntyped: typed call throws") {
  expectEqual("net:refused", typedAndUntyped(-1, 10))
}
GuardCatchTests.test("typedAndUntyped: untyped call throws (same concrete type at runtime)") {
  expectEqual("net:refused", typedAndUntyped(1, -1))
}

// MARK: labeled continue and break as catch exits inside loops.

func catchContinue(_ values: [Int]) -> [Int] {
  var results: [Int] = []
  for v in values {
    guard let n = try typedThrow(v) catch { continue }
    results.append(n)
  }
  return results
}

GuardCatchTests.test("catchContinue: skips throwing elements") {
  expectEqual([1, 3], catchContinue([1, -1, 3]))
}
GuardCatchTests.test("catchContinue: empty when all throw") {
  expectEqual([], catchContinue([-1, -2]))
}

func catchBreak(_ values: [Int]) -> Int {
  var sum = 0
  outer: for v in values {
    guard let n = try typedThrow(v) catch { break outer }
    sum += n
  }
  return sum
}

GuardCatchTests.test("catchBreak: stops on first throw") {
  expectEqual(3, catchBreak([1, 2, -1, 4]))
}
GuardCatchTests.test("catchBreak: full loop when nothing throws") {
  expectEqual(6, catchBreak([1, 2, 3]))
}

// MARK: catch body that re-throws — shape (2).

func catchOnlyRethrow(_ x: Int) throws -> String {
  guard let n = try sometimesNil(x) else {
    return "nil"
  } catch {
    throw error
  }
  return "v=\(n)"
}

GuardCatchTests.test("catchOnlyRethrow: success path") {
  do {
    expectEqual("v=5", try catchOnlyRethrow(5))
  } catch {
    expectUnreachable("unexpected throw: \(error)")
  }
}
GuardCatchTests.test("catchOnlyRethrow: nil routes to else, no throw") {
  do {
    expectEqual("nil", try catchOnlyRethrow(0))
  } catch {
    expectUnreachable("unexpected throw: \(error)")
  }
}
GuardCatchTests.test("catchOnlyRethrow: catch body re-throws to caller") {
  var caught = false
  do {
    _ = try catchOnlyRethrow(-1)
  } catch {
    caught = true
  }
  expectEqual(true, caught)
}

// MARK: typed re-throw — catch body re-throws a typed error.

func typedCatchOnlyRethrow(_ x: Int) throws(NetErr) -> Int {
  guard let n = try typedSometimesNil(x) else {
    return -1
  } catch {
    throw error
  }
  return n
}

GuardCatchTests.test("typedCatchOnlyRethrow: success") {
  do {
    expectEqual(5, try typedCatchOnlyRethrow(5))
  } catch {
    expectUnreachable("unexpected throw: \(error)")
  }
}
GuardCatchTests.test("typedCatchOnlyRethrow: nil to else") {
  do {
    expectEqual(-1, try typedCatchOnlyRethrow(0))
  } catch {
    expectUnreachable("unexpected throw: \(error)")
  }
}
GuardCatchTests.test("typedCatchOnlyRethrow: re-throws typed error") {
  var caught = false
  do {
    _ = try typedCatchOnlyRethrow(-1)
  } catch NetErr.refused {
    caught = true
  } catch {
    expectUnreachable("wrong error type: \(error)")
  }
  expectEqual(true, caught)
}

// MARK: catch clauses don't exhaustively catch thrown errors

func nonexhaustiveCatches(_ n: Int) throws -> String {
  guard
    let num1 = try typedThrow(n),         // typed throwing function with non-optional result
    let n = try mayThrow(num1)            // throwing function with non-optional result
  catch _ as NetErr {
    return "neterr"
  }
  return String(n)
}

GuardCatchTests.test("nonexhaustive: success path") {
  expectEqual("2", try? nonexhaustiveCatches(2))
}
GuardCatchTests.test("nonexhaustive: caught error path") {
  expectEqual("neterr", try? nonexhaustiveCatches(Sentinels.typedThrow))
}
GuardCatchTests.test("nonexhaustive: uncaught error path") {
  expectEqual(nil, try? nonexhaustiveCatches(Sentinels.mayThrow))
}

runAllTests()
