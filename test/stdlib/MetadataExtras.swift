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

// RUN: %target-run-simple-swift
// REQUIRES: executable_test

import StdlibUnittest
import SwiftPrivateMetadataExtras

var tests = TestSuite("MetadataExtras")

struct TestStruct {
    var int = 0
    var double = 0.0
    var bool = false
}

class BaseClass {
    var superInt = 0
    init() {}
}

class TestClass: BaseClass {
    var int = 0
    var double = 0.0
    var bool = false
    override init() {}
}

tests.test("applyFields_TestStruct") {
    var count = 0
    
    expectTrue(applyFields(of: TestStruct.self) {
        charPtr, offset, type in
        count += 1
        switch String(cString: charPtr) {
        case "int":
            return offset == 0 && type == Int.self
        case "double":
            return offset == 8 && type == Double.self
        case "bool":
            return offset == 16 && type == Bool.self
        default:
            return false
        }
    })
    
    expectEqual(count, 3)

    // Applying to struct type with .classType option fails
    expectFalse(applyFields(of: TestStruct.self, options: .classType) { _, _, _ in true })
}

tests.test("applyFields_TestClass") {
    var count = 0
    
    expectTrue(applyFields(of: TestClass.self, options: .classType) {
        charPtr, offset, type in
        count += 1
        switch String(cString: charPtr) {
        case "superInt":
            return offset == 16 && type == Int.self
        case "int":
            return offset == 24 && type == Int.self
        case "double":
            return offset == 32 && type == Double.self
        case "bool":
            return offset == 40 && type == Bool.self
        default:
            return false
        }
    })
    
    expectEqual(count, 4)

    // Applying to class type without .classType option fails
    expectFalse(applyFields(of: TestClass.self) { _, _, _ in true })
}

runAllTests()
