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

#include "Metadata_Metadata.h"

#include <dlfcn.h>
#include <memory>
#include <objc/runtime.h>

//namespace SPME {
namespace swift {

namespace {

struct opaque_existential_container: uncopyable {
    uintptr_t buffer[3];
    const metadata *type;
    // witness tables follow.

    bool is_value_inline() const;
    const void *project_value() const;
};

struct class_existential_container: uncopyable {
    void *value;
    // witness tables follow.
};

inline bool
opaque_existential_container::is_value_inline() const
{
    return !(type->flags() & value_witness_table::is_non_inline);
}

size_t
mangled_type_name_size(const char *ptr)
{
    auto end = (const uint8_t *)ptr;
    while (*end != 0) {
        auto current = *end++;
        // Skip over a symbolic reference
        if (current >= 0x1 && current <= 0x17) {
            end += 4;
        } else if (current >= 0x18 && current <= 0x1F) {
            end += sizeof(intptr_t);
        }
    }
    return ((const char *)end) - ptr;
}

struct type_name {
    const char *data;
    uintptr_t length;
};

} // anonymous namespace

// Swift runtime functions.

extern "C" {
const void *swift_conformsToProtocol(const metadata *type,
                                     const void *protocol_descriptor);

const metadata *swift_getObjCClassMetadata(const metadata *type);

const metadata *swift_getTypeByMangledNameInContext(const char *mangled_name,
                                                    size_t mangled_name_size, const void *context, const void *generic_args);

type_name swift_getTypeName(const metadata *type, bool qualified)
spme_swift_cc;
}

const context_descriptor *
metadata::descriptor() const
{
    switch (canonical_kind()) {
        case kind_struct:
        case kind_enum:
        case kind_optional:
            return ((const nominal_type_metadata *)this)->descriptor;
        case kind_class:
            if (auto m = ((const any_class_type_metadata *)this)->swift_metadata()) {
                return m->descriptor;
            }
            // fall through
        default:
            return nullptr;
    }
}

const nominal_type_descriptor *
context_descriptor::nominal_descriptor() const
{
    switch (flags & context_descriptor::kind_mask) {
        case context_descriptor::kind_struct:
        case context_descriptor::kind_enum:
            return (const nominal_type_descriptor *)this;
        default:
            return nullptr;
    }
}

const class_type_descriptor *
context_descriptor::class_descriptor() const
{
    switch (flags & context_descriptor::kind_mask) {
        case context_descriptor::kind_class:
            return (const class_type_descriptor *)this;
        default:
            return nullptr;
    }
}

const nominal_type_descriptor *
metadata::nominal_descriptor() const
{
    if (auto desc = descriptor()) {
        return desc->nominal_descriptor();
    }
    
    return nullptr;
}

extern "C" char $sSQMp; // protocol descriptor for Swift.Equatable

const equatable_conformance *
metadata::equatable() const
{
    return reinterpret_cast<const equatable_conformance *>(
                                                           swift_conformsToProtocol(this, &$sSQMp));
}

inline bool
metadata::visit_struct(metadata_visitor &visitor) const
{
    auto descriptor = this->nominal_descriptor();
    if (!descriptor || !descriptor->reflection_metadata
        || descriptor->struct_type.field_offset_vector_offset == 0)
    {
        return visitor.unknown_result();
    }
    
    auto field_records = descriptor->reflection_metadata->records;
    const uint32_t *field_offsets = (const uint32_t *) ((const uintptr_t *)this
                                                        + descriptor->struct_type.field_offset_vector_offset);
    
    for (size_t i = 0; i < descriptor->struct_type.field_count; i++) {
        if (!visitor.visit_field(this, field_records[i], field_offsets[i])) {
            return false;
        }
    }
    
    return true;
}

inline bool
metadata::visit_tuple(metadata_visitor &visitor) const
{
    auto tuple = (const tuple_type_metadata *)this;
    
    for (size_t i = 0; i < tuple->num_elements; i++) {
        if (tuple->elements[i].type) {
            auto type = tuple->elements[i].type;
            auto offset = tuple->elements[i].offset;
            if (!visitor.visit_element(type, ref_kind::strong, offset)) {
                return false;
            }
        }
    }
    
    return true;
}

inline bool
metadata::visit_enum(metadata_visitor &visitor) const
{
    auto descriptor = this->nominal_descriptor();
    if (!descriptor || !descriptor->reflection_metadata) {
        return visitor.unknown_result();
    }
    
    auto field_records = descriptor->reflection_metadata->records;
    unsigned cases = descriptor->reflection_metadata->num_fields;
    unsigned payloads = descriptor->enum_type.payload_cases & 0x00ffffffU;
    spme_assert(payloads + descriptor->enum_type.empty_cases == cases);
    
    if (payloads == 0) {
        // treat as opaque data.
        return visitor.unknown_result();
    }
    
    for (unsigned i = 0; i < cases; i++) {
        // ignore cases without payloads.
        if (field_records[i].mangled_type_name) {
            if (!visitor.visit_case(this, field_records[i], i)) {
                return false;
            }
        }
    }
    
    return true;
}

bool
metadata::visit(metadata_visitor &visitor) const
{
    switch (canonical_kind()) {
        case kind_class:
            return visitor.visit_class((const any_class_type_metadata *)this);
        case kind_struct:
            return visit_struct(visitor);
        case kind_enum:
        case kind_optional:
            return visit_enum(visitor);
        case kind_tuple:
            return visit_tuple(visitor);
        case kind_function:
            return visitor.visit_function((const function_type_metadata *)this);
        case kind_existential:
            return visitor.visit_existential((const existential_type_metadata *)this);
        case kind_opaque: {
            static const metadata *native_object = mangled_type_name_ref("Bo");
            if (this == native_object) {
                return visitor.visit_native_object(this);
            }
            return visitor.unknown_result();
        }
        default:
            return visitor.unknown_result();
    }
}

inline bool
metadata::visit_heap_class(metadata_visitor &visitor) const
{
    auto cls = ((const any_class_type_metadata *)this)->swift_metadata();
    if (!cls) {
        return visitor.unknown_result();
    }
    
    auto desc = cls->descriptor;
    if (!desc) {
        return visitor.unknown_result();
    }
    
    // ignore _SwiftObject base that fails the is_swift_class() check.
    if (desc->superclass_type) {
        if (auto s_cls = cls->superclass) {
            // visit superclass fields first, to preserve ordering.
            if (s_cls->canonical_kind() != kind_class) {
                return visitor.unknown_result();
            } else if (!s_cls->visit_heap_class(visitor)) {
                return false;
            }
        }
    }
    
    auto fields = desc->fields.get();
    if (!fields || fields->num_fields == 0) {
        return true;
    }

    if (fields->num_fields != desc->num_fields) {
        return visitor.unknown_result();
    }

    // If the class has ObjC heritage, get the field offset using the
    // ObjC metadata, because we don't update the field offsets in the
    // face of resilient base classes. (Adapted from swift runtime
    // ReflectionMirror.mm)
    const uintptr_t *offsets = nullptr;
    if (cls->flags & class_type_metadata::uses_swift_refcounting) {
        offsets = cls->field_offset_vector();
    } else {
        unsigned int ivar_count = 0;
        std::unique_ptr<Ivar, free_deleter> ivars(
                                                  class_copyIvarList((Class)cls, &ivar_count));
        if (ivars && ivar_count == fields->num_fields) {
            auto p = (uintptr_t *)alloca(ivar_count * sizeof(uintptr_t));
            for (unsigned i = 0; i < ivar_count; i++) {
                p[i] = ivar_getOffset(ivars.get()[i]);
            }
            offsets = p;
        }
    }

    if (!offsets || offsets[0] == 0) {
        return visitor.unknown_result();
    }

    for (size_t i = 0; i < fields->num_fields; i++) {
        if (!visitor.visit_field(this, fields->records[i], offsets[i])) {
            return false;
        }
    }
    
    return true;
}

inline bool
metadata::visit_heap_box(metadata_visitor &visitor) const
{
    auto metadata = (const generic_box_heap_metadata *)this;
    
    if (auto type = metadata->boxed_type) {
        size_t offset = metadata->offset;
        auto align = type->alignment_mask();
        offset = (offset + align) & ~align;
        return visitor.visit_element(type, ref_kind::strong, offset);
    }
    
    return visitor.unknown_result();
}

inline bool
metadata::visit_heap_locals(metadata_visitor &visitor) const
{
    auto type = (const heap_local_variable_metadata *)this;
    
    auto desc = type->descriptor;
    if (!desc) {
        return visitor.unknown_result();
    }
    
    // FIXME: ignore any box with indirect metadata sources, demangling
    // the types without being able to supply the correct subsitution map
    // sometimes crashes: rdar://problem/47144629.
    
    if (desc->num_metadata_sources) {
        return visitor.unknown_result();
    }
    
    size_t offset = type->offset_to_first_capture;
    
    // offset_to_first_capture can sometimes be zero, in which case start
    // from the standard header offset.
    
    if (offset == 0) {
        offset = 2 * sizeof(uintptr_t);
    }
    
    // "Bindings" are captured generic types, they're laid out at the
    // head of the box, each one is a metadata pointer. We just treat
    // them as pointers to compare.
    
    for (unsigned int i = 0; i < desc->num_bindings; i++) {
        static auto pointer_type = mangled_type_name_ref("Bp"); // RawPointer
        if (!pointer_type) {
            return visitor.unknown_result();
        }
        auto success = visitor.visit_element(pointer_type,
                                             ref_kind::unowned_unsafe, offset);
        if (!success) {
            return false;
        }
        offset += sizeof(uintptr_t);
    }
    
    // Captured variable values follow.
    
    for (unsigned i = 0; i < desc->num_capture_types; i++) {
        auto &elt = desc->capture_type(i);
        metadata::ref_kind ref;
        auto elt_type = mangled_type_name_ref(elt.mangled_type_name.get(), &ref);
        if (!elt_type) {
            return visitor.unknown_result();
        }
        auto align = elt_type->alignment_mask();
        offset = (offset + align) & ~align;
        if (!visitor.visit_element(elt_type, ref, offset)) {
            return false;
        }
        offset += elt_type->size();
    }
    
    return true;
}

bool
metadata::visit_heap(metadata_visitor &visitor, uint32_t mask) const
{
    switch (canonical_kind()) {
        case kind_class:
            if (mask & heap_kind_mask_class) {
                return visit_heap_class(visitor);
            }
            break;
        case kind_heap_local_variable:
            if (mask & heap_kind_mask_local) {
                return visit_heap_locals(visitor);
            }
            break;
        case kind_heap_generic_local_variable:
            if (mask & heap_kind_mask_generic) {
                return visit_heap_box(visitor);
            }
            break;
        case kind_error_object:
            break;
        default:
            break;
    }
    
    return visitor.unknown_result();
}

bool
metadata_visitor::visit_field(const metadata *struct_type,
                              const field_record &field, size_t offset)
{
    if (auto type_name = field.mangled_type_name.get()) {
        metadata::ref_kind ref;
        if (auto type = struct_type->mangled_type_name_ref(type_name, &ref)) {
            return visit_element(type, ref, offset);
        }
    }
    
    return unknown_result();
}

const metadata *
metadata::mangled_type_name_ref(const char *type_name,
                                metadata::ref_kind *ret_ref_kind) const
{
    if (!type_name) {
        return nullptr;
    }
    
    const void *context = nullptr, *generic_args = nullptr;
    
    if (auto desc = this->descriptor()) {
        context = desc;
        if (desc->flags & context_descriptor::is_generic) {
            switch (desc->flags & context_descriptor::kind_mask) {
                case context_descriptor::kind_struct:
                case context_descriptor::kind_enum:
                    generic_args = ((const nominal_type_metadata *)this)->generic_args;
                    break;
                case context_descriptor::kind_class:
                    generic_args = ((const uintptr_t *)this +
                                    ((const class_type_descriptor *)desc)->generic_argument_offset());
                    break;
                default:
                    spme_assert(false);
            }
        }
    }
    
    auto len = mangled_type_name_size(type_name);
    
    auto ret = swift_getTypeByMangledNameInContext(type_name, len,
                                                   context, generic_args);
    
    if (ret && ret_ref_kind) {
        *ret_ref_kind = ref_kind::strong;
        
        // Looking for these mangling rules:
        //
        // type ::= type 'Xo' -- @unowned type
        // type ::= type 'Xu' -- @unowned(unsafe) type
        // type ::= type 'Xw' -- @weak type
        //
        // for our uses it seems ok to assume they're always last.
        
        if (len > 2 && type_name[len - 2] == 'X') {
            switch (type_name[len - 1]) {
                case 'o':
                    *ret_ref_kind = ref_kind::unowned;
                    break;
                case 'u':
                    *ret_ref_kind = ref_kind::unowned_unsafe;
                    break;
                case 'w':
                    *ret_ref_kind = ref_kind::weak;
                    break;
            }
        }
    }
    
    return ret;
}

const char *
metadata::name(bool qualified) const
{
    return swift_getTypeName(this, qualified).data;
}

intptr_t
class_type_descriptor::immediate_members_offset() const
{
    if (!has_resilient_superclass()) {
        return (kind_specific_flags() & KindFlags::immediate_members_negative) ?
        -intptr_t(non_resilient_super.metadata_negative_size_in_words) :
        intptr_t(non_resilient_super.metadata_positive_size_in_words
                 - num_immediate_members);
    } else {
        return resilient_super.immediate_members_offset;
    }
}

intptr_t
class_type_descriptor::field_offset_vector_offset() const
{
    if (!has_resilient_superclass()) {
        return _field_offset_vector_offset;
    } else {
        return (immediate_members_offset() / sizeof(uintptr_t)) +
        _field_offset_vector_offset;
    }
}

const void *
opaque_existential_container::project_value() const
{
    if (!(type->flags() & value_witness_table::is_non_inline)) {
        return (const void *)buffer;
    }
    
    auto align = type->alignment_mask();
    auto offset = (2 * sizeof(uintptr_t) + align) & ~align;
    
    return (const uint8_t *)buffer[0] + offset;
}

existential_type_metadata::representation_type
existential_type_metadata::representation() const
{
    if (((flags & special_protocol_mask) >> special_protocol_shift) != 0) {
        return representation_type::unsupported;
    } else if (!(flags & non_class_constraint_flag)) {
        return representation_type::class_type;
    } else {
        return representation_type::opaque;
    }
}

const void *
existential_type_metadata::project_value(const void *container) const
{
    switch (representation()) {
        case representation_type::class_type:
            // return the _pointer_ to the class.
            return &((const class_existential_container *)container)->value;
        case representation_type::opaque:
            return ((const opaque_existential_container *)container)->project_value();
        case representation_type::unsupported:
            return nullptr;
    }
}

const metadata *
existential_type_metadata::dynamic_type(const void *container) const
{
    switch (representation()) {
        case representation_type::class_type: {
            // return the _pointer_ to the class.
            auto obj = ((const class_existential_container *)container)->value;
            auto cls_type = (const any_class_type_metadata *)object_getClass((id)obj);
            if (cls_type->is_swift_class()) {
                return cls_type;
            }
            // objc metaclass: convert to swift metatype.
            return swift_getObjCClassMetadata(cls_type); }
        case representation_type::opaque: {
            auto box = (const opaque_existential_container *)container;
            return box->type; }
        case representation_type::unsupported:
            return nullptr;
    }
}

} // namespace swift
//} // namespace SPME
