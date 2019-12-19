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

import SwiftShims

extension SPMETypeID {
    @_transparent
    internal init(_ type: Any.Type) {
        self.init(value: unsafeBitCast(type, to: UnsafeRawPointer.self))
    }
    
    var type: Any.Type {
        @_transparent get {
            return unsafeBitCast(value, to: Any.Type.self)
        }
    }
}

public struct ApplyOptions: OptionSet {
    public var rawValue: UInt32
    
    public init(rawValue: UInt32) {
        self.rawValue = rawValue
    }
    
    /// If set the top-level type is required to be a class. If unset the
    /// top-level type is required to be a struct or tuple.
    public static var classType = ApplyOptions(rawValue: 1 << 0)
    
    /// If set the presence of things that can't be introspected won't
    /// cause the function to immediately return failure.
    public static var ignoreUnknown = ApplyOptions(rawValue: 1 << 1)
}

extension SPMETypeApplyOptions {
    init(_ options: ApplyOptions) {
        self.init(rawValue: options.rawValue)
    }
}
