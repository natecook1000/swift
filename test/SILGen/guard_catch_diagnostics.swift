// RUN: %target-swift-frontend -emit-sil -verify %s

func mayThrow() throws -> Int { 0 }

func fallthroughCatch() {
  guard let _ = try mayThrow() catch {
    _ = 0
  } // expected-error {{'catch' clause of a 'guard' statement must not fall through, consider using a 'return' or 'throw' to exit the scope}}
}

// Properly exiting via return — no diagnostic.
func returnFromCatch() -> Int {
  guard let n = try mayThrow() catch { return -1 }
  return n
}

// Properly exiting via throw — no diagnostic.
func throwFromCatch() throws -> Int {
  guard let n = try mayThrow() catch { throw error }
  return n
}
