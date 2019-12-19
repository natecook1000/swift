//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_STDLIB_SHIMS_METADATAEXTRAS_H_
#define SWIFT_STDLIB_SHIMS_METADATAEXTRAS_H_

#include "SwiftStdint.h"

#if __has_feature(ptrauth_calls)
# define SPME_PTRAUTH 1
# include <ptrauth.h>
#endif

//#include <tuple>

#if defined(SPME_PTRAUTH)
# define spme_swift_function_ptr(k) __ptrauth_swift_function_pointer(k)
#else
# define spme_swift_function_ptr(k)
#endif

#define SPME_CONCAT(a, b) SPME_CONCAT__(a, b)
#define SPME_CONCAT__(a, b) a##b

#if !defined(SPME_PTRAUTH)
# define SPME_CLOSURE_TYPE(...) _SPME_CLOSURE_TYPE_HELPER(__VA_ARGS__)
# define _SPME_CLOSURE_TYPE_HELPER(Key, Ret, ...) SPME::ClosureFunction<Ret, __VA_ARGS__>
# define SPME_CLOSURE_KEYS(f) f()
#else
# define SPME_CLOSURE_TYPE(...) _SPME_CLOSURE_TYPE_HELPER(__VA_ARGS__)
# define _SPME_CLOSURE_TYPE_HELPER(Key, Ret, ...) SPME_CONCAT(SPME::ClosureFunction, Key)<Ret, __VA_ARGS__>
# define SPME_CLOSURE_KEYS(f) f(VV) f(FV) f(AK)
#endif

#define SPME_DECLARE_CLOSURE_TYPE(key) template<typename Ret, typename ...Args> struct ClosureFunction##key;

#define SPME_SWIFT_CC __attribute__ ((swiftcall))
#define SPME_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define SPME_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")

/// In-memory representation of a Swift closure.
typedef struct _SPMEClosureStorage SPMEClosureStorage;

struct _SPMEClosureStorage {
    const void *thunk;
    const void *__nullable context;
};

#define spme_swift_cc SPME_SWIFT_CC
#define spme_swift_context __attribute__ ((swift_context))

#if (__cplusplus)
namespace SPME {

// Function object to call a swift closure with direct parameters and
// results.

// __ptrauth() can't take its value from a template parameter, have to
// burn it into the struct definition, hence macro craziness.

#define SPME_DEFINE_CLOSURE_TYPE(key)                                   \
template<typename Ret, typename ...Args>                                \
struct ClosureFunction##key {                                           \
  union {                                                               \
    const void *ptr;                                                    \
    Ret (* spme_swift_function_ptr(SPME_CLOSURE_KEY_##key) fun)         \
      (const Args ..., const void *spme_swift_context) spme_swift_cc;   \
  } thunk;                                                              \
  const void *context;                                                  \
  constexpr ClosureFunction##key() {                                    \
    thunk.ptr = nullptr; context = nullptr;                             \
  }                                                                     \
  ClosureFunction##key(const SPMEClosureStorage &s) {                   \
    thunk.ptr = s.thunk; context = s.context;                           \
  }                                                                     \
  ClosureFunction##key &operator=(const SPMEClosureStorage &s) {        \
    thunk.ptr = s.thunk; context = s.context; return *this;             \
  }                                                                     \
  inline Ret operator()(Args... args) const {                           \
    return thunk.fun(args..., context);                                 \
  }                                                                     \
  explicit operator SPMEClosureStorage() const {                        \
    return {.thunk = thunk.ptr, .context = context};                    \
  }                                                                     \
  operator bool() const {return thunk.ptr != nullptr;}                  \
  bool operator!() const {return thunk.ptr == nullptr;}                 \
  ClosureFunction##key &ref() {swift_retain(context); return *this;}    \
  void unref() {swift_release(context);}                                \
};

#if defined(SPME_PTRAUTH)
# define SPME_CLOSURE_KEY_VV 0x0f08 // () -> Void
# define SPME_CLOSURE_KEY_FV 0x1e3e // (() -> Void) -> Void
# define SPME_CLOSURE_KEY_AK 0x8758 // SPMETypeApplyFields
#endif

SPME_CLOSURE_KEYS(SPME_DEFINE_CLOSURE_TYPE)

} // namespace SPME
#endif

#if __has_attribute(enum_extensibility)
#define __SPME_OPTIONS_ATTRIBUTES __attribute__((flag_enum,enum_extensibility(open)))
#else
#error Compiler does not support enum_extensibility
#endif

#if (__cplusplus)
#define SPME_OPTIONS(_type, _name) _type _name; enum __SPME_OPTIONS_ATTRIBUTES : _type
#else
#define SPME_OPTIONS(_type, _name) enum __SPME_OPTIONS_ATTRIBUTES _name : _type _name; enum _name : _type
#endif

#ifdef __cplusplus
namespace swift { extern "C" {
#endif

typedef struct {const void *__nonnull value;} SPMETypeID;

/// Options for SPMETypeApplyFields().
typedef SPME_OPTIONS(__swift_uint32_t, SPMETypeApplyOptions) {
  /// If set the top-level type is required to be a class. If unset the
  /// top-level type is required to be a struct or tuple.
  SPMETypeApplyClassType = 1U << 0,
  /// If set the presence of things that can't be introspected won't
  /// cause the function to immediately return failure.
  SPMETypeApplyIgnoreUnknown = 1U << 1,
};

#ifdef __cplusplus
}} // extern "C", namespace swift
#endif

#endif /* SWIFT_STDLIB_SHIMS_METADATAEXTRAS_H_ */
