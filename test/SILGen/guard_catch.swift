// RUN: %target-swift-emit-silgen -parse-as-library -module-name guard_catch %s | %FileCheck %s

// SILGen patterns for the trailing-catch shapes of 'guard'. The patterns
// below check the structural skeleton — a try_apply to the throwing call
// with a normal/error split, the success branch binding the value and
// continuing, and the error branch routing into an owned error parameter
// on a postmatter-style catch dispatch block.

enum E: Error { case bad }
public enum NetErr: Error { case timeout, refused }

func mayThrow() throws -> Int { 0 }
func mayThrowOpt() throws -> Int? { 0 }
func voidThrow() throws {}
func optInt() -> Int? { 0 }

@inline(never)
public func typedMayThrow() throws(NetErr) -> Int { 0 }

// MARK: catch-only guard binding a non-Optional throwing call.

// CHECK-LABEL: sil {{.*}} @$s11guard_catch0B5OnlyNSiyKF
// CHECK:        try_apply {{.*}} : $@convention(thin) () -> (Int, @error any Error), normal [[SUCCESS:bb[0-9]+]], error [[ERROR:bb[0-9]+]]
// CHECK:      [[SUCCESS]]({{.*}} : $Int):
// CHECK:        move_value [var_decl]
// CHECK:        debug_value {{.*}}, let, name "n"
// CHECK:      [[ERROR]]({{.*}} : @owned $any Error):
// CHECK:        debug_value {{.*}}, let, name "error"
public func catchOnlyN() throws -> Int {
  guard let n = try mayThrow() catch { return -1 }
  return n
}

// MARK: catch-only guard with bare void-throwing element.

// CHECK-LABEL: sil {{.*}} @$s11guard_catch0B11OnlyVoidEltyyKF
// CHECK:        try_apply {{.*}} : $@convention(thin) () -> @error any Error, normal [[OK:bb[0-9]+]], error [[ERR:bb[0-9]+]]
// CHECK:      [[OK]]({{.*}} : $()):
// CHECK:      [[ERR]]({{.*}} : @owned $any Error):
public func catchOnlyVoidElt() throws {
  guard try voidThrow() catch { return }
}

// MARK: guard with else + catches with reachable catch.

// CHECK-LABEL: sil {{.*}} @$s11guard_catch12elseAndCatchySSSiF
// CHECK:        try_apply {{.*}} : $@convention(thin) (Int) -> (Optional<Int>, @error any Error), normal [[S2OK:bb[0-9]+]], error [[S2ERR:bb[0-9]+]]
// The success branch is followed by an Optional unwrap (switch_enum on
// the optional initializer's result) since the binding is refutable.
// CHECK:      [[S2OK]]({{.*}} : $Optional<Int>):
// CHECK:        switch_enum {{.*}}, case #Optional.some!enumelt: {{bb[0-9]+}}, case #Optional.none!enumelt: {{bb[0-9]+}}
// CHECK:      [[S2ERR]]({{.*}} : @owned $any Error):
// CHECK:        debug_value {{.*}}, let, name "error"
public func elseAndCatch(_ x: Int) -> String {
  guard let v = try sometimesNil(x) else {
    return "nil"
  } catch {
    return "err"
  }
  return "\(v)"
}

// Helper for the test above; declared here so the SILGen patterns above
// don't get inlined or specialized away.
@inline(never)
public func sometimesNil(_ x: Int) throws -> Int? {
  if x < 0 { throw E.bad }
  return x == 0 ? nil : x
}

// MARK: catch-only guard with typed throws.
//
// With a typed-throws call the error parameter carries the concrete type
// directly — no existential boxing needed.

// CHECK-LABEL: sil {{.*}} @$s11guard_catch0B9OnlyTypedSiyF
// CHECK:        try_apply {{.*}} : $@convention(thin) () -> (Int, @error NetErr), normal [[OK:bb[0-9]+]], error [[ERR:bb[0-9]+]]
// CHECK:      [[OK]]({{.*}} : $Int):
// CHECK:        move_value [var_decl]
// CHECK:        debug_value {{.*}}, let, name "n"
// CHECK:      [[ERR]]({{.*}} : $NetErr):
// CHECK:        debug_value {{.*}}, let, name "error"
public func catchOnlyTyped() -> Int {
  guard let n = try typedMayThrow() catch { return -1 }
  return n
}

// MARK: catch-only guard with multiple throwing conditions.
//
// Each condition produces its own try_apply; errors from either branch
// route to the same catch dispatch block.

// CHECK-LABEL: sil {{.*}} @$s11guard_catch0B9OnlyMultiSiyF
// CHECK:        try_apply {{.*}} normal {{bb[0-9]+}}, error {{bb[0-9]+}}
// CHECK:        try_apply {{.*}} normal {{bb[0-9]+}}, error {{bb[0-9]+}}
// CHECK:        debug_value {{.*}}, let, name "error"
public func catchOnlyMulti() -> Int {
  guard let a = try mayThrow(),
        let b = try mayThrow()
  catch { return -1 }
  return a + b
}
