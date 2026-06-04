// RUN: %target-swift-frontend -parse -verify %s

enum E: Error { case bad }

func mayThrow() throws -> Int { 0 }
func mayThrowOpt() throws -> Int? { 0 }
func voidThrow() throws {}
func optInt() -> Int? { 0 }

// MARK: shape (1) — classic, unchanged.

func classic(_ x: Int?) -> Int {
  guard let n = x else { return -1 }
  return n
}

// MARK: shape (2) — else + catches.

func elseAndOneCatch() throws -> Int {
  guard let v = try mayThrowOpt() else {
    return -1
  } catch {
    return -2
  }
  return v
}

func elseAndMultipleCatches() throws -> Int {
  guard let v = try mayThrowOpt() else {
    return -1
  } catch E.bad {
    return -2
  } catch {
    return -3
  }
  return v
}

// MARK: catch-only guards.

func catchOnlyVoid() throws {
  guard try voidThrow() catch { return }
}

func catchOnlyBinding() throws -> Int {
  guard let n = try mayThrow() catch { return -1 }
  return n
}

func catchOnlyMultiple() throws -> Int {
  guard let n = try mayThrow() catch E.bad {
    return -1
  } catch {
    return -2
  }
  return n
}

// MARK: malformed input.

// Missing both else and catch — the existing missing-else diagnostic fires.
func missingBoth(_ x: Int?) -> Int {
  guard let _ = x // expected-error {{expected 'else' after 'guard' condition}}
}

// `catch` before `else` — the parser treats 'catch { return -1 }' as the
// (only) trailing catches list, leaving the spurious 'else { return -2 }'
// at the brace-item level.
// TODO: diagnose as out of order; fixme should swap else before catch blocks
func catchBeforeElse() throws -> Int? {
  guard let v = try mayThrowOpt() catch { return -1 } else { return -2 } // expected-error {{consecutive statements on a line must be separated by ';'}} expected-error {{expected expression}}
  return v
}

// MARK: 'where' clause in catch patterns.

func whereInElseAndCatch() throws -> Int {
  guard let v = try mayThrowOpt() else {
    return -1
  } catch E.bad where true {
    return -2
  } catch {
    return -3
  }
  return v
}

func whereInCatchOnly() throws -> Int {
  guard let n = try mayThrow() catch E.bad where true {
    return -1
  } catch {
    return -2
  }
  return n
}
