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

#include "../../public/SwiftShims/MetadataExtras.h"
#include "../../public/SwiftShims/SwiftStddef.h"
#include "Metadata_Metadata.h"

namespace swift { extern "C" {

/// Calls `callback(name, offset, type)` for each field of the Swift
/// type `type`. Returns true if all fields were successfully visited
/// (guided by value of `options` value).
bool SPMETypeApplyFields(SPMETypeID type_id, SPMETypeApplyOptions flags, SPME_CLOSURE_TYPE(AK, bool, const char *, __swift_uint32_t, SPMETypeID) callback) SPME_SWIFT_CC {

    using Callback = SPME_CLOSURE_TYPE(AK, bool, const char *, __swift_uint32_t, SPMETypeID);
    
    struct Visitor: swift::metadata_visitor {
        Callback &callback;
        SPMETypeApplyOptions flags;

        Visitor(Callback &c, SPMETypeApplyOptions f): callback(c), flags(f) {}

        bool unknown_result() const override {
            // ignore missing fields if requested
            return bool(flags & SPMETypeApplyIgnoreUnknown);
        }
                
        bool visit_field(const swift::metadata *type,
                         const swift::field_record &field, size_t offset) override
        {
            auto ft = type->mangled_type_name_ref(field.mangled_type_name.get());
            if (ft) {
                return callback(field.field_name.get(), offset, {.value = ft});
            }
            return unknown_result();
        }
    };
    
    Visitor v(callback, flags);

    auto type = (const swift::metadata *)type_id.value;

    switch (type->canonical_kind()) {
    case swift::metadata::kind_struct:
    case swift::metadata::kind_tuple:
      if (flags & SPMETypeApplyClassType) {
          return false;
      }
      return type->visit(v);
    case swift::metadata::kind_class:
      if (!(flags & SPMETypeApplyClassType)) {
        return false;
      }
      return type->visit_heap(v, swift::metadata::heap_kind_mask_class);
    default:
      return false;
    }
}

} } // extern "C"; namespace swift
