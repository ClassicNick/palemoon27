/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/EffectiveAddressAnalysis.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace jit;

static void
AnalyzeLsh(TempAllocator& alloc, MLsh* lsh)
{
    if (lsh->specialization() != MIRType_Int32)
        return;

    MDefinition* index = lsh->lhs();
    MOZ_ASSERT(index->type() == MIRType_Int32);

    MDefinition* shift = lsh->rhs();
    if (!shift->isConstantValue())
        return;

    Value shiftValue = shift->constantValue();
    if (!shiftValue.isInt32() || !IsShiftInScaleRange(shiftValue.toInt32()))
        return;

    Scale scale = ShiftToScale(shiftValue.toInt32());

    int32_t displacement = 0;
    MInstruction* last = lsh;
    MDefinition* base = nullptr;
    while (true) {
        if (!last->hasOneUse())
            break;

        MUseIterator use = last->usesBegin();
        if (!use->consumer()->isDefinition() || !use->consumer()->toDefinition()->isAdd())
            break;

        MAdd* add = use->consumer()->toDefinition()->toAdd();
        if (add->specialization() != MIRType_Int32 || !add->isTruncated())
            break;

        MDefinition* other = add->getOperand(1 - add->indexOf(*use));

        if (other->isConstantValue()) {
            displacement += other->constantValue().toInt32();
        } else {
            if (base)
                break;
            base = other;
        }

        last = add;
    }

    if (!base) {
        uint32_t elemSize = 1 << ScaleToShift(scale);
        if (displacement % elemSize != 0)
            return;

        if (!last->hasOneUse())
            return;

        MUseIterator use = last->usesBegin();
        if (!use->consumer()->isDefinition() || !use->consumer()->toDefinition()->isBitAnd())
            return;

        MBitAnd* bitAnd = use->consumer()->toDefinition()->toBitAnd();
        MDefinition* other = bitAnd->getOperand(1 - bitAnd->indexOf(*use));
        if (!other->isConstantValue() || !other->constantValue().isInt32())
            return;

        uint32_t bitsClearedByShift = elemSize - 1;
        uint32_t bitsClearedByMask = ~uint32_t(other->constantValue().toInt32());
        if ((bitsClearedByShift & bitsClearedByMask) != bitsClearedByMask)
            return;

        bitAnd->replaceAllUsesWith(last);
        return;
    }

    MEffectiveAddress* eaddr = MEffectiveAddress::New(alloc, base, index, scale, displacement);
    last->replaceAllUsesWith(eaddr);
    last->block()->insertAfter(last, eaddr);
}

static bool
IsAlignmentMask(uint32_t m)
{
    // Test whether m is just leading ones and trailing zeros.
    return (-m & ~m) == 0;
}

template<typename MAsmJSHeapAccessType>
static void
AnalyzeAsmHeapAccess(MAsmJSHeapAccessType *ins, MIRGraph &graph)
{
    MDefinition *ptr = ins->ptr();

    if (ptr->isConstantValue()) {
        // Look for heap[i] where i is a constant offset, and fold the offset.
        // By doing the folding now, we simplify the task of codegen; the offset
        // is always the address mode immediate. This also allows it to avoid
        // a situation where the sum of a constant pointer value and a non-zero
        // offset doesn't actually fit into the address mode immediate.
        int32_t imm = ptr->constantValue().toInt32();
        if (imm != 0 && ins->tryAddDisplacement(imm)) {
            MInstruction *zero = MConstant::New(graph.alloc(), Int32Value(0));
            ins->block()->insertBefore(ins, zero);
            ins->replacePtr(zero);
        }
    } else if (ptr->isAdd()) {
        // Look for heap[a+i] where i is a constant offset, and fold the offset.
        MDefinition *op0 = ptr->toAdd()->getOperand(0);
        MDefinition *op1 = ptr->toAdd()->getOperand(1);
        if (op0->isConstantValue())
            mozilla::Swap(op0, op1);
        if (op1->isConstantValue()) {
            int32_t imm = op1->constantValue().toInt32();
            if (ins->tryAddDisplacement(imm))
                ins->replacePtr(op0);
        }
    } else if (ptr->isBitAnd() && ptr->hasOneUse()) {
        // Transform heap[(a+i)&m] to heap[(a&m)+i] so that we can fold i into
        // the access. Since we currently just mutate the BitAnd in place, this
        // requires that we are its only user.
        MDefinition *lhs = ptr->toBitAnd()->getOperand(0);
        MDefinition *rhs = ptr->toBitAnd()->getOperand(1);
        int lhsIndex = 0;
        if (lhs->isConstantValue()) {
            mozilla::Swap(lhs, rhs);
            lhsIndex = 1;
        }
        if (lhs->isAdd() && rhs->isConstantValue()) {
            MDefinition *op0 = lhs->toAdd()->getOperand(0);
            MDefinition *op1 = lhs->toAdd()->getOperand(1);
            if (op0->isConstantValue())
                mozilla::Swap(op0, op1);
            if (op1->isConstantValue()) {
                uint32_t i = op1->constantValue().toInt32();
                uint32_t m = rhs->constantValue().toInt32();
                if (IsAlignmentMask(m) && ((i & m) == i) && ins->tryAddDisplacement(i))
                    ptr->toBitAnd()->replaceOperand(lhsIndex, op0);
            }
        }
    }
}

// This analysis converts patterns of the form:
//   truncate(x + (y << {0,1,2,3}))
//   truncate(x + (y << {0,1,2,3}) + imm32)
// into a single lea instruction, and patterns of the form:
//   asmload(x + imm32)
//   asmload(x << {0,1,2,3})
//   asmload((x << {0,1,2,3}) + imm32)
//   asmload((x << {0,1,2,3}) & mask)            (where mask is redundant with shift)
//   asmload(((x << {0,1,2,3}) + imm32) & mask)  (where mask is redundant with shift + imm32)
// into a single asmload instruction (and for asmstore too).
//
// Additionally, we should consider the general forms:
//   truncate(x + y + imm32)
//   truncate((y << {0,1,2,3}) + imm32)
bool
EffectiveAddressAnalysis::analyze()
{
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        for (MInstructionIterator i = block->begin(); i != block->end(); i++) {
            // Note that we don't check for MAsmJSCompareExchangeHeap
            // or MAsmJSAtomicBinopHeap, because the backend and the OOB
            // mechanism don't support non-zero offsets for them yet.
            if (i->isLsh())
                AnalyzeLsh(graph_.alloc(), i->toLsh());
            else if (i->isAsmJSLoadHeap())
                AnalyzeAsmHeapAccess(i->toAsmJSLoadHeap(), graph_);
            else if (i->isAsmJSStoreHeap())
                AnalyzeAsmHeapAccess(i->toAsmJSStoreHeap(), graph_);
        }
    }
    return true;
}
