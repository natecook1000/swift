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

#ifndef SPME_METADATA_METADATA_H
#define SPME_METADATA_METADATA_H

#ifndef SPME_INTERNAL_H
#define SPME_INTERNAL_H

#include "../../public/SwiftShims/MetadataExtras.h"
#include "../../public/SwiftShims/SwiftStddef.h"
#include <stdlib.h>

namespace swift {

/// Emits an assertion that will be removed from release builds. Both
/// the failing condition and readable error message `fmt, ...` will be
/// printed, although the readable message may be omitted.
#define spme_assert(cond) do {} while(0)

/// helper type to delete copy-ctor and assignment operator.
struct uncopyable {
protected:
    uncopyable() __attribute__ ((nothrow)) {}
    ~uncopyable() {}
public:
    uncopyable(const uncopyable &rhs) = delete;
    uncopyable &operator=(const uncopyable &rhs) = delete;
};

// for use with std::unique_ptr.

struct free_deleter {
    void operator()(void *p) const {
        free(p);
    }
};

} // namespace swift

#endif // SPME_INTERNAL_H

#if defined(SPME_PTRAUTH)
# define spme_swift_value_witness_function_ptr(k) __ptrauth_swift_value_witness_function_pointer(k)
# define spme_swift_type_ptr __ptrauth(ptrauth_key_process_independent_data, 1, 0xae86)
# define spme_swift_heap_object_destructor __ptrauth_swift_heap_object_destructor
#else
# define spme_swift_value_witness_function_ptr(k)
# define spme_swift_type_ptr
# define spme_swift_heap_object_destructor
#endif

//namespace SPME {
namespace swift {

template<typename T>
struct relative_ptr {
  int32_t _offset;

  operator bool() const {return _offset != 0;}
  bool operator!() const {return _offset == 0;}

  const T *get() const {
    return _offset == 0 ? nullptr
      : (_offset & (alignof(T) - 1)) == 1
      ? *(const T **)(((uintptr_t)this + _offset) & ~(alignof(T) - 1))
      : (const T *)((const uint8_t *)this + _offset);
  }

  const T *operator->() const {return get();}
  const T &operator*() const {return &get();}
  const T &operator[](size_t i) const {return get()[i];}
};

struct equatable_conformance;
struct metadata_visitor;
struct value_witness_table;
struct context_descriptor;
struct nominal_type_descriptor;
struct class_type_descriptor;

struct metadata: uncopyable {
    enum Kind: uintptr_t {
        // flags
        kind_is_non_type = 0x400,
        kind_is_non_heap = 0x200,
        kind_is_runtime_private = 0x100,
        // types
        kind_class = 0, // nominal_type
        kind_struct = 0 | kind_is_non_heap, // nominal_type
        kind_enum = 1 | kind_is_non_heap, // nominal_type
        kind_optional = 2 | kind_is_non_heap, // nominal_type
        kind_foreign_class = 3 | kind_is_non_heap,
        kind_opaque = 0 | kind_is_runtime_private | kind_is_non_heap,
        kind_tuple = 1 | kind_is_runtime_private | kind_is_non_heap,
        kind_function = 2 | kind_is_runtime_private | kind_is_non_heap,
        kind_existential = 3 | kind_is_runtime_private | kind_is_non_heap,
        kind_metatype = 4 | kind_is_runtime_private | kind_is_non_heap,
        kind_objc_class_wrapper = 5 | kind_is_runtime_private | kind_is_non_heap,
        kind_existential_metatype = 6 | kind_is_runtime_private | kind_is_non_heap,
        kind_heap_local_variable = 0 | kind_is_non_type,
        kind_heap_generic_local_variable = 0 | kind_is_non_type |
        kind_is_runtime_private,
        kind_error_object = 1 | kind_is_non_type | kind_is_runtime_private,
        kind_last_enumerated = 0x7FF,
    };
    
    Kind kind;
    
    /// Converts anything greater than last-enumerated to class.
    Kind canonical_kind() const;
    
    const value_witness_table &value_witness() const;
    
    size_t size() const;
    size_t stride() const;
    uint32_t flags() const;
    size_t alignment_mask() const;
    bool is_POD() const;
    bool is_bitwise_takable() const;
    
    const context_descriptor *descriptor() const;
    const nominal_type_descriptor *nominal_descriptor() const;
    
    const equatable_conformance *equatable() const;
    
    const char *name(bool qualified = false) const;
    
    /// Applies `visitor` to each struct field or enum payload in the
    /// type. Note that this doesn't walk into class fields, instead it
    /// calls `visitor.visit_class()` with the class's metadata, allowing
    /// visit_class() to be called if wanted.
    bool visit(metadata_visitor &visitor) const;
    
    /// Mask bits for visit_heap().
    enum HeapKindMask {
        heap_kind_mask_class = 1U << 0,
        heap_kind_mask_local = 1U << 1,
        heap_kind_mask_generic = 1U << 2,
        heap_kind_all = ~0U,
    };
    
    /// Applies `visitor` to each field of whatever is stored behind the
    /// reference type described by `this`. Returns false if the visitor
    /// returned false, or if the contents of the type could not be
    /// introspected (e.g. not a reference type, or an objc class).
    bool visit_heap(metadata_visitor &visitor, uint32_t mask) const;
    
    enum class ref_kind {
        strong,
        weak,
        unowned,
        unowned_unsafe
    };
    
    // using `this` as context for local types, may return null.
    const metadata *mangled_type_name_ref(const char *type_name,
                                          ref_kind *ret_ref_kind = nullptr) const;
    
private:
    bool visit_struct(metadata_visitor &visitor) const;
    bool visit_tuple(metadata_visitor &visitor) const;
    bool visit_enum(metadata_visitor &visitor) const;
    
    bool visit_heap_class(metadata_visitor &visitor) const;
    bool visit_heap_box(metadata_visitor &visitor) const;
    bool visit_heap_locals(metadata_visitor &visitor) const;
};

struct field_record;

struct equatable_conformance: uncopyable {
    bool operator()(const void *lhs, const void *rhs, const metadata *type)
    const;
};

struct any_class_type_metadata;
struct existential_type_metadata;
struct function_type_metadata;

struct metadata_visitor {
    virtual bool unknown_result() const {
        return false;
    }

    virtual bool visit_element(const metadata *type, metadata::ref_kind ref_kind, size_t offset) {
        return unknown_result();
    }
    
    // base implementation looks up field type and calls visit_element().
    virtual bool visit_field(const metadata *struct_type, const field_record &field, size_t offset);
    
    virtual bool visit_case(const metadata *enum_type, const field_record &field, unsigned tag) {
        return unknown_result();
    }
    
    virtual bool visit_class(const any_class_type_metadata *class_type) {
        return unknown_result();
    }
    
    virtual bool visit_existential(const existential_type_metadata *type) {
        return unknown_result();
    }
    
    virtual bool visit_function(const function_type_metadata *type) {
        return unknown_result();
    }
    
    virtual bool visit_native_object(const metadata *type) {
        return unknown_result();
    }
};

struct value_witness_table: uncopyable {
    enum Flags: uint32_t {
        alignment_mask = 0x000000FF,
        is_non_POD = 0x00010000,
        is_non_inline = 0x00020000,
        has_spare_bits = 0x00080000,
        is_non_bitwise_takable = 0x00100000,
        has_enum_witnesses = 0x00200000,
        incomplete = 0x00400000,
    };
    
    void *(* spme_swift_value_witness_function_ptr(0xda4a) init_buffer_with_copy_of_buffer)
    (void *dest, const void *src, metadata *type);
    
    void (* spme_swift_value_witness_function_ptr(0x04f8) destroy)
    (void *object, const metadata *type);
    
    void *(* spme_swift_value_witness_function_ptr(0xe3ba) init_with_copy)
    (void *dest, const void *src, const metadata *type);
    
    void *(* spme_swift_value_witness_function_ptr(0x8751) assign_with_copy)
    (void *dest, const void *src, const metadata *type);
    
    void *(* spme_swift_value_witness_function_ptr(0x48d8) init_with_take)
    (void *dest, const void *src, const metadata *type);
    
    void *(* spme_swift_value_witness_function_ptr(0xefda) assign_with_take)
    (void *dest, const void *src, const metadata *type);
    
    unsigned (* spme_swift_value_witness_function_ptr(0x60f0) get_enum_single_tccs_payload)
    (const void *src, unsigned empty_cases, const metadata *type);
    
    void (* spme_swift_value_witness_function_ptr(0xa0d1) store_enum_single_tccs_payload)
    (void *dst, unsigned which_case, unsigned empty_cases, const metadata *type);
    
    uintptr_t size;
    uintptr_t stride;
    uint32_t flags;
    uint32_t extra_inhabitant_count;
    
    // only if value_witness_has_enum_witnesses is set in flags.
    
    unsigned (* spme_swift_value_witness_function_ptr(0xa3b5) get_enum_tag)
    (const void *obj, const metadata *type);
    
    void (* spme_swift_value_witness_function_ptr(0x041d) destructive_project_enum_data)
    (void *obj, const metadata *type);
    
    void (* spme_swift_value_witness_function_ptr(0xb2e4) destructive_inject_enum_tag)
    (void *obj, unsigned tag, const metadata *type);
};

struct nominal_type_metadata: metadata {
    const nominal_type_descriptor *spme_swift_type_ptr descriptor;
    void *generic_args[1];
    // field offset vector follows generic args (if present).
};

struct context_descriptor: uncopyable {
    enum ContextFlags: uint32_t {
        is_generic = 0x80,
        is_unique = 0x40,
        kind_mask = 0x1f,
        kind_module = 0,
        kind_extension = 1,
        kind_anonymous = 2,
        kind_protocol = 3,
        kind_class = 0x10,
        kind_struct = 0x11,
        kind_enum = 0x12,
    };
    
    ContextFlags flags;
    relative_ptr<context_descriptor> parent;
    
    const nominal_type_descriptor *nominal_descriptor() const;
    const class_type_descriptor *class_descriptor() const;
    
    uint32_t kind_specific_flags() const;
};

struct field_descriptor;

struct nominal_type_descriptor: context_descriptor {
    relative_ptr<char> name;
    relative_ptr<int> metadata_accessor;
    relative_ptr<field_descriptor> reflection_metadata;
    
    union {
        struct {
            uint32_t field_count;
            uint32_t field_offset_vector_offset;
        } struct_type;
        
        struct {
            uint32_t payload_cases; // 8.24, and-payload-size-offset
            uint32_t empty_cases;
        } enum_type;
    };
};

struct field_record: uncopyable {
    enum Flags: uint32_t {
        is_indirect_case = 1,
        is_var = 2,
    };
    
    Flags flags;
    relative_ptr<char> mangled_type_name;
    relative_ptr<char> field_name;
};

struct field_descriptor: uncopyable {
    enum class Kind: uint16_t {
        struct_,
        class_,
        enum_,
        multi_payload_enum,
        protocol,
        objc_protocol,
        objc_class,
    };
    
    relative_ptr<char> mangled_type_name;
    relative_ptr<char> superclass;
    Kind kind;
    uint16_t field_record_size;
    uint32_t num_fields;
    field_record records[1];
};

struct tuple_type_element: uncopyable {
    const metadata *type;
    size_t offset;
};

struct tuple_type_metadata: metadata {
    size_t num_elements;
    relative_ptr<char> labels;
    tuple_type_element elements[1];
};

struct class_type_metadata;

struct any_class_type_metadata: metadata {
    const any_class_type_metadata *superclass;
    uintptr_t cache_data[2];
    size_t data;
    
    // is this object valid swift metadata?
    bool is_swift_class() const;
    
    const class_type_metadata *swift_metadata() const;
};

struct class_type_metadata: any_class_type_metadata {
    enum TypeFlags: uint32_t {
        is_swift_pre_stable_ABI = 0x1,
        uses_swift_refcounting = 0x2,
        has_custom_objc_name = 0x4,
    };
    
    // fields below only valid if is_swift_class() is true.
    
    TypeFlags flags;
    
    /// The address point of instances of this type.
    uint32_t instance_address_point;
    /// The required size of instances of this type.
    /// 'InstanceAddressPoint' bytes go before the address point;
    /// 'InstanceSize - InstanceAddressPoint' bytes go after it.
    uint32_t instance_size;
    /// The alignment mask of the address point of instances of this
    /// type.
    uint16_t instance_align_mask;
    uint16_t reserved;
    
    /// The total size of the class object, including prefix and suffix
    /// extents.
    uint32_t class_size;
    /// The offset of the address point within the class object.
    uint32_t class_address_point;
    
    // may be null.
    const class_type_descriptor *spme_swift_type_ptr descriptor;
    
    using ClassIVarDestroyer = spme_swift_cc void(spme_swift_context void *);
    
    /// A function for destroying instance variables, used to clean up
    /// after an early return from a constructor. If null, no clean up
    /// will be performed and all ivars must be trivial.
    ClassIVarDestroyer *spme_swift_heap_object_destructor ivar_destroyer;
    
    // After this come the class members, laid out as follows:
    //   - class members for the superclass (recursively)
    //   - metadata reference for the parent, if applicable
    //   - generic parameters for this class
    //   - class variables (if we choose to support these)
    //   - "tabulated" virtual methods
    
    const uintptr_t *field_offset_vector() const;
};

struct class_type_descriptor: context_descriptor {
    struct KindFlags {
        static constexpr uint32_t has_import_info = 1U << 2;
        static constexpr uint32_t immediate_members_negative = 1U << 12;
        static constexpr uint32_t has_resilient_superclass = 1U << 13;
        static constexpr uint32_t has_override_table = 1U << 14;
        static constexpr uint32_t has_vtable = 1U << 15;
    };
    
    relative_ptr<char> name;
    relative_ptr<void *> access_function;
    relative_ptr<field_descriptor> fields;
    
    relative_ptr<char> superclass_type;
    
    union {
        /// If this descriptor does not have a resilient superclass:
        struct {
            /// negative size of metadata objects of this class (in words).
            uint32_t metadata_negative_size_in_words;
            /// positive size of metadata objects of this class (in words).
            uint32_t metadata_positive_size_in_words;
        } non_resilient_super;
        
        /// If this descriptor has a resilient superclass:
        struct {
            // actually the bounds cache, but we only want the offset,
            // which is the only safe part to access non-atomically.
            const intptr_t &immediate_members_offset;
        } resilient_super;
    };
    
    /// The number of additional members added by this class to the class
    /// metadata.  This data is opaque by default to the runtime, other
    /// than as exposed in other members; it's really just
    /// NumImmediateMembers * sizeof(void*) bytes of data.
    ///
    /// Whether those bytes are added before or after the address point
    /// depends on areImmediateMembersNegative().
    uint32_t num_immediate_members;
    
    /// The number of stored properties in the class, not including its
    /// superclasses. If there is a field offset vector, this is its
    /// length.
    uint32_t num_fields;
    
    /// If non-zero, offset in words. If this class has a resilient
    /// superclass, this offset is relative to the size of the resilient
    /// superclass metadata. Otherwise, it is absolute.
    uint32_t _field_offset_vector_offset;
    
    bool has_resilient_superclass() const;
    
    bool has_field_offset_vector() const;
    intptr_t field_offset_vector_offset() const;
    
    intptr_t generic_argument_offset() const;
    
private:
    intptr_t immediate_members_offset() const;
};

struct existential_type_metadata: metadata {
    enum class representation_type {
        opaque, class_type, unsupported
    };
    
    enum Flags: uint32_t {
        num_witness_tables_mask = 0x00ffffffU,
        non_class_constraint_flag = 0x80000000U, // zero if class
        has_superclass_flag = 0x40000000U,
        special_protocol_mask = 0x3f000000U,
        special_protocol_shift = 24U,
    };
    
    Flags flags;
    uint32_t num_protocols;
    
    bool is_class_bound() const;
    representation_type representation() const;
    
    // returns null for unsupported representations (e.g. errors)
    const metadata *dynamic_type(const void *container) const;
    
    // returns null for unsupported representations (e.g. errors)
    void *project_value(void *container) const;
    const void *project_value(const void *container) const;
};

struct function_type_metadata: metadata {
    enum Flags: uintptr_t {
        num_parameters_mask = 0x0000ffffU,
        convention_mask = 0x00ff0000U,
        convention_swift = 0x00000000U,
        convention_block = 0x00010000U,
        convention_thin = 0x00020000U,
        convention_c = 0x00040000U,
        throws = 0x01000000U,
        has_param_flags = 0x02000000U,
        is_escaping = 0x04000000U,
    };
    
    Flags flags;
    const swift::metadata *result_type;
    const swift::metadata *parameters[1];
    
    size_t num_parameters() const;
    uint32_t parameter_flags(size_t i) const;
    
    bool is_convention_swift() const;
};

struct capture_descriptor;

struct heap_local_variable_metadata: metadata {
    uint32_t offset_to_first_capture;
    const capture_descriptor *descriptor;
};

struct capture_descriptor: uncopyable {
    struct CaptureType {
        relative_ptr<char> mangled_type_name;
    };
    
    struct MetadataSource {
        relative_ptr<char> mangled_type_name;
        relative_ptr<char> mangled_metadata_source;
    };
    
    /// The number of captures in the closure and the number of mangled
    /// type names in capture_types.
    uint32_t num_capture_types;
    /// The number of sources of metadata available in the
    /// MetadataSourceMap directly following the list of capture's
    /// typerefs.
    uint32_t num_metadata_sources;
    /// The number of items in the NecessaryBindings structure at the
    /// head of the closure.
    uint32_t num_bindings;
    
    const CaptureType &capture_type(size_t i) const;
    const MetadataSource &metadata_source(size_t i) const;
};

struct generic_box_heap_metadata: metadata {
    unsigned offset;
    const metadata *boxed_type;
};

// implementation details

inline metadata::Kind
metadata::canonical_kind() const
{
    return kind > kind_last_enumerated ? kind_class : kind;
}

inline const value_witness_table &
metadata::value_witness() const
{
    return *((const value_witness_table **)this)[-1];
}

inline size_t
metadata::size() const
{
    return value_witness().size;
}

inline uint32_t
metadata::flags() const
{
    return value_witness().flags;
}

inline size_t
metadata::stride() const
{
    return value_witness().stride;
}

inline size_t
metadata::alignment_mask() const
{
    return flags() & value_witness_table::alignment_mask;
}

inline bool
metadata::is_POD() const
{
    return !(flags() & value_witness_table::is_non_POD);
}

inline bool
metadata::is_bitwise_takable() const
{
    return !(flags() & value_witness_table::is_non_bitwise_takable);
}

inline uint32_t
context_descriptor::kind_specific_flags() const
{
    return (flags >> 16) & 0xffff;
}

inline bool
any_class_type_metadata::is_swift_class() const
{
    return bool(data & 3);
}

inline const class_type_metadata *
any_class_type_metadata::swift_metadata() const
{
    return is_swift_class() ? (const class_type_metadata *)this : nullptr;
}

inline const uintptr_t *
class_type_metadata::field_offset_vector() const
{
    return !descriptor->has_field_offset_vector() ? nullptr :
    (const uintptr_t *)this + descriptor->field_offset_vector_offset();
}

inline bool
class_type_descriptor::has_resilient_superclass() const
{
    return bool(kind_specific_flags()
                & KindFlags::has_resilient_superclass);
}

inline bool
class_type_descriptor::has_field_offset_vector() const
{
    return _field_offset_vector_offset != 0;
}

inline intptr_t
class_type_descriptor::generic_argument_offset() const
{
    return immediate_members_offset();
}

inline bool
existential_type_metadata::is_class_bound() const
{
    return representation() == representation_type::class_type;
}

inline void *
existential_type_metadata::project_value(void *container) const
{
    return const_cast<void *>(project_value(const_cast<const void *>(container)));
}

inline size_t
function_type_metadata::num_parameters() const
{
    return flags & num_parameters_mask;
}

inline uint32_t
function_type_metadata::parameter_flags(size_t i) const
{
    if (!(flags & has_param_flags)) {
        return 0;
    }
    return *(uint32_t *)(parameters + num_parameters());
}

inline bool
function_type_metadata::is_convention_swift() const
{
    return (flags & convention_mask) == convention_swift;
}

inline const capture_descriptor::CaptureType &
capture_descriptor::capture_type(size_t i) const
{
    spme_assert(i < num_capture_types);
    return ((const CaptureType *)(this + 1))[i];
}

inline const capture_descriptor::MetadataSource &
capture_descriptor::metadata_source(size_t i) const
{
    spme_assert(i < num_metadata_sources);
    auto base = ((const CaptureType *)(this + 1)) + num_capture_types;
    return ((const MetadataSource *)base)[i];
}

extern "C" bool SPMEDispatchEquatable(const void *lhs, const void *rhs, const metadata *type, const equatable_conformance *wt) spme_swift_cc;

inline bool
equatable_conformance::operator()(const void *lhs, const void *rhs, const metadata *type) const
{
    return SPMEDispatchEquatable(lhs, rhs, type, this);
}

} // namespace swift
//} // namespace SPME

#endif /* SPME_METADATA_METADATA_H */
