// RUN: %target-typecheck-verify-swift

// Sema-level coverage for trailing-catch shapes of 'guard'.

enum NetErr: Error { case timeout, refused }
enum OtherErr: Error { case bad }

func mayThrow() throws -> Int { 0 }
func mayThrowOpt() throws -> Int? { 0 }
func voidThrow() throws {}
func optInt() -> Int? { 0 }
func mayThrowTyped() throws(NetErr) -> Int { 0 }
func mayThrowOther() throws(OtherErr) -> Int { 0 }
func mayThrowTypedOpt() throws(NetErr) -> Int? { 0 }

// MARK: catch-only guards.

func catchOnlyBinding() throws -> Int {
  guard let n = try mayThrow() catch { return -1 }
  return n            // 'n' is Int (irrefutable; no Optional unwrap).
}

func catchOnlyBindingTypeIsInt() throws {
  guard let n = try mayThrow() catch { return }
  let _: Int = n      // Confirms type is Int, not Int?.
}

func catchOnlyVoid() throws {
  guard try voidThrow() catch { return }
}

func catchOnlyMixed() throws -> Int {
  guard try voidThrow(), let n = try mayThrow() catch { return -1 }
  return n
}

// MARK: shape (2) — else + catches.

func shape2BindingPlusCatch() throws -> Int {
  guard let v = try mayThrowOpt() else { return -1 } catch { return -2 }
  return v
}

func shape2BindingPlusVoidThrow() -> Int {
  guard let n = optInt(), try voidThrow() else { return -1 } catch { return -2 }
  return n
}

func shape2TypedThrows() throws -> Int {
  guard let v = try mayThrowOpt() else { return -1 } catch NetErr.timeout {
    return -2
  } catch {
    return -3
  }
  return v
}

// MARK: 'guard always succeeds' is suppressed for guards with catches.

func alwaysSucceedsSuppressed() throws {
  guard try voidThrow() catch { return }
}

// MARK: optional binding in catch-only guard must have 'else'.

func catchOnlyOptionalBinding() throws {
  guard let _ = try mayThrowOpt() catch { return } // expected-error {{catch-only 'guard' cannot bind a value of optional type 'Int?'; add an 'else' clause to handle the nil case}}
}

// MARK: typed-error type inference.

func typedCatchOnly() {
  guard let _ = try mayThrowTyped() catch {
    let _: NetErr = error
    return
  }
}

func typedTwoDifferent() {
  guard let _ = try mayThrowTyped(),
        let _ = try mayThrowOther()
  catch {
    let _: NetErr = error // expected-error {{cannot convert value of type 'any Error' to specified type 'NetErr'}}
    return
  }
}

// MARK: typed-error type inference — shape (2).

func typedShape2TwoSame() {
  guard let _ = try mayThrowTypedOpt(),
        let _ = try mayThrowTypedOpt()
  else { return }
  catch {
    let _: NetErr = error
    return
  }
}

func typedShape2TwoDifferent() {
  guard let _ = try mayThrowTypedOpt(),
        let _ = try mayThrowOther()
  else { return }
  catch {
    let _: NetErr = error // expected-error {{cannot convert value of type 'any Error' to specified type 'NetErr'}}
    return
  }
}

// MARK: try? and try! suppress routing to catch.

func tryOptionalUnreachable() {
  guard let _ = try? mayThrow() else { return } catch { // expected-warning {{'catch' block is unreachable because no errors are thrown in 'guard' condition}}
    return
  }
}

func tryBangUnreachable() {
  guard let _ = try! mayThrow() catch { // expected-warning {{'catch' block is unreachable because no errors are thrown in 'guard' condition}}
    return
  }
}

// MARK: catch-only guard consumes throwing effects; enclosing function need not declare 'throws'.

func catchOnlyInNonThrowingFunc() -> Int {
  guard let n = try mayThrow() catch { return -1 }
  return n
}

func shape2InNonThrowingFunc() -> Int {
  guard let n = try mayThrowOpt() else { return -1 } catch { return -2 }
  return n
}

// MARK: named binding in catch pattern.

func namedCatchBinding() {
  guard let _ = try mayThrowTyped() catch let e {
    let _: NetErr = e
    return
  }
}

func namedCatchBindingWithCast() {
  guard let _ = try mayThrow() catch let e as NetErr {
    let _: NetErr = e
    return
  } catch {
    return
  }
}

// MARK: 'where' clause in catch pattern.

func catchWhereClause(flag: Bool) {
  guard let _ = try mayThrowTyped() catch NetErr.timeout where flag {
    return
  } catch {
    return
  }
}
