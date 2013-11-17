/* The file is part of Snowman decompiler.             */
/* See doc/licenses.txt for the licensing information. */

//
// SmartDec decompiler - SmartDec is a native code to C/C++ decompiler
// Copyright (C) 2015 Alexander Chernov, Katerina Troshina, Yegor Derevenets,
// Alexander Fokin, Sergey Levin, Leonid Tsvetkov
//
// This file is part of SmartDec decompiler.
//
// SmartDec decompiler is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SmartDec decompiler is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SmartDec decompiler.  If not, see <http://www.gnu.org/licenses/>.
//

#include "Hooks.h"

#include <cassert>

#include <nc/common/Foreach.h>
#include <nc/common/Range.h>

#include <nc/core/arch/Instruction.h>
#include <nc/core/ir/BasicBlock.h>
#include <nc/core/ir/Function.h>
#include <nc/core/ir/Statements.h>

#include "Conventions.h"
#include "DescriptorAnalyzer.h"
#include "CallHook.h"
#include "CallingConvention.h"
#include "EnterHook.h"
#include "Signature.h"
#include "ReturnHook.h"

namespace nc {
namespace core {
namespace ir {
namespace cconv {

Hooks::Hooks(const Conventions &conventions):
    conventions_(conventions)
{}

Hooks::~Hooks() {}

CalleeId Hooks::getCalleeId(const Function *function) const {
    assert(function != NULL);

    if (function->entry() && function->entry()->address()) {
        return CalleeId(CalleeId::ENTRY_ADDRESS, *function->entry()->address());
    } else {
        return CalleeId();
    }
}

CalleeId Hooks::getCalleeId(const Call *call) const {
    assert(call != NULL);

    if (auto addr = getCalledAddress(call)) {
        return CalleeId(CalleeId::ENTRY_ADDRESS, *addr);
    } else if (call->instruction()) {
        return CalleeId(CalleeId::CALL_ADDRESS, call->instruction()->addr());
    } else {
        return CalleeId();
    }
}

boost::optional<ByteAddr> Hooks::getCalledAddress(const Call *call) const {
    assert(call != NULL);

    return nc::find_optional(call2address_, call);
}

void Hooks::setCalledAddress(const Call *call, ByteAddr addr) {
    assert(call != NULL);

    call2address_[call] = addr;
}

const CallingConvention *Hooks::getCallingConvention(const CalleeId &calleeId) {
    if (!calleeId) {
        return NULL;
    }
    if (auto result = conventions_.getCallingConvention(calleeId)) {
        return result;
    } else {
        conventionDetector_(calleeId);
        return conventions_.getCallingConvention(calleeId);
    }
}

DescriptorAnalyzer *Hooks::getDescriptorAnalyzer(const CalleeId &calleeId) {
    if (!calleeId) {
        return NULL;
    }
    if (!nc::contains(id2analyzer_, calleeId)) {
        if (const CallingConvention *callingConvention = getCallingConvention(calleeId)) {
            id2analyzer_[calleeId] = callingConvention->createDescriptorAnalyzer();
        }
    }
    return nc::find(id2analyzer_, calleeId).get();
}

EnterHook *Hooks::getEnterHook(const Function *function) {
    assert(function != NULL);

    auto calleeId = getCalleeId(function);
    if (!calleeId) {
        return NULL;
    }

    auto key = std::make_pair(calleeId, function);
    if (!nc::contains(function2analyzer_, key)) {
        if (DescriptorAnalyzer *descriptorAnalyzer = getDescriptorAnalyzer(calleeId)) {
            function2analyzer_[key] = descriptorAnalyzer->createEnterHook(function);
        }
    }
    return nc::find(function2analyzer_, key).get();
}

CallHook *Hooks::getCallHook(const Call *call) {
    assert(call != NULL);

    auto calleeId = getCalleeId(call);
    if (!calleeId) {
        return NULL;
    }

    auto key = std::make_pair(calleeId, call);
    if (!nc::contains(call2analyzer_, key)) {
        if (DescriptorAnalyzer *descriptorAnalyzer = getDescriptorAnalyzer(calleeId)) {
            call2analyzer_[key] = descriptorAnalyzer->createCallHook(call);
        }
    }
    return nc::find(call2analyzer_, key).get();
}

ReturnHook *Hooks::getReturnHook(const Function *function, const Return *ret) {
    assert(ret != NULL);

    auto calleeId = getCalleeId(function);
    if (!calleeId) {
        return NULL;
    }

    auto key = std::make_pair(calleeId, ret);
    if (!nc::contains(return2analyzer_, key)) {
        if (DescriptorAnalyzer *addressAnalyzer = getDescriptorAnalyzer(calleeId)) {
            return2analyzer_[key] = addressAnalyzer->createReturnHook(ret);
        }
    }
    return nc::find(return2analyzer_, key).get();
}

} // namespace cconv
} // namespace ir
} // namespace core
} // namespace nc

/* vim:set et sts=4 sw=4: */
