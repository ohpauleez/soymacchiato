/*
 * Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package java.lang.invoke;

import sun.invoke.util.VerifyType;
import sun.invoke.util.Wrapper;
import java.util.Arrays;
import static java.lang.invoke.MethodHandleNatives.Constants.*;
import static java.lang.invoke.MethodHandleStatics.*;

/**
 * This method handle performs simple conversion or checking of a single argument.
 * @author jrose
 */
class AdapterMethodHandle extends BoundMethodHandle {

    //MethodHandle vmtarget;   // next AMH or BMH in chain or final DMH
    //Object       argument;   // parameter to the conversion if needed
    //int          vmargslot;  // which argument slot is affected
    private final int conversion;  // the type of conversion: RETYPE_ONLY, etc.

    // Constructors in this class *must* be package scoped or private.
    private AdapterMethodHandle(MethodHandle target, MethodType newType,
                long conv, Object convArg) {
        super(newType, convArg, newType.parameterSlotDepth(1+convArgPos(conv)));
        this.conversion = convCode(conv);
        // JVM might update VM-specific bits of conversion (ignore)
        MethodHandleNatives.init(this, target, convArgPos(conv));
    }
    private AdapterMethodHandle(MethodHandle target, MethodType newType,
                long conv) {
        this(target, newType, conv, null);
    }

    // TO DO:  When adapting another MH with a null conversion, clone
    // the target and change its type, instead of adding another layer.

    /** Can a JVM-level adapter directly implement the proposed
     *  argument conversions, as if by MethodHandles.convertArguments?
     */
    static boolean canPairwiseConvert(MethodType newType, MethodType oldType) {
        // same number of args, of course
        int len = newType.parameterCount();
        if (len != oldType.parameterCount())
            return false;

        // Check return type.  (Not much can be done with it.)
        Class<?> exp = newType.returnType();
        Class<?> ret = oldType.returnType();
        if (!VerifyType.isNullConversion(ret, exp))
            return false;

        // Check args pairwise.
        for (int i = 0; i < len; i++) {
            Class<?> src = newType.parameterType(i); // source type
            Class<?> dst = oldType.parameterType(i); // destination type
            if (!canConvertArgument(src, dst))
                return false;
        }

        return true;
    }

    /** Can a JVM-level adapter directly implement the proposed
     *  argument conversion, as if by MethodHandles.convertArguments?
     */
    static boolean canConvertArgument(Class<?> src, Class<?> dst) {
        // ? Retool this logic to use RETYPE_ONLY, CHECK_CAST, etc., as opcodes,
        // so we don't need to repeat so much decision making.
        if (VerifyType.isNullConversion(src, dst)) {
            return true;
        } else if (src.isPrimitive()) {
            if (dst.isPrimitive())
                return canPrimCast(src, dst);
            else
                return canBoxArgument(src, dst);
        } else {
            if (dst.isPrimitive())
                return canUnboxArgument(src, dst);
            else
                return true;  // any two refs can be interconverted
        }
    }

    /**
     * Create a JVM-level adapter method handle to conform the given method
     * handle to the similar newType, using only pairwise argument conversions.
     * For each argument, convert incoming argument to the exact type needed.
     * Only null conversions are allowed on the return value (until
     * the JVM supports ricochet adapters).
     * The argument conversions allowed are casting, unboxing,
     * integral widening or narrowing, and floating point widening or narrowing.
     * @param newType required call type
     * @param target original method handle
     * @return an adapter to the original handle with the desired new type,
     *          or the original target if the types are already identical
     *          or null if the adaptation cannot be made
     */
    static MethodHandle makePairwiseConvert(MethodType newType, MethodHandle target) {
        MethodType oldType = target.type();
        if (newType == oldType)  return target;

        if (!canPairwiseConvert(newType, oldType))
            return null;
        // (after this point, it is an assertion error to fail to convert)

        // Find last non-trivial conversion (if any).
        int lastConv = newType.parameterCount()-1;
        while (lastConv >= 0) {
            Class<?> src = newType.parameterType(lastConv); // source type
            Class<?> dst = oldType.parameterType(lastConv); // destination type
            if (VerifyType.isNullConversion(src, dst)) {
                --lastConv;
            } else {
                break;
            }
        }
        // Now build a chain of one or more adapters.
        MethodHandle adapter = target;
        MethodType midType = oldType.changeReturnType(newType.returnType());
        for (int i = 0; i <= lastConv; i++) {
            Class<?> src = newType.parameterType(i); // source type
            Class<?> dst = midType.parameterType(i); // destination type
            if (VerifyType.isNullConversion(src, dst)) {
                // do nothing: difference is trivial
                continue;
            }
            // Work the current type backward toward the desired caller type:
            if (i != lastConv) {
                midType = midType.changeParameterType(i, src);
            } else {
                // When doing the last (or only) real conversion,
                // force all remaining null conversions to happen also.
                assert(VerifyType.isNullConversion(newType, midType.changeParameterType(i, src)));
                midType = newType;
            }

            // Tricky case analysis follows.
            // It parallels canConvertArgument() above.
            if (src.isPrimitive()) {
                if (dst.isPrimitive()) {
                    adapter = makePrimCast(midType, adapter, i, dst);
                } else {
                    adapter = makeBoxArgument(midType, adapter, i, dst);
                }
            } else {
                if (dst.isPrimitive()) {
                    // Caller has boxed a primitive.  Unbox it for the target.
                    // The box type must correspond exactly to the primitive type.
                    // This is simpler than the powerful set of widening
                    // conversions supported by reflect.Method.invoke.
                    // Those conversions require a big nest of if/then/else logic,
                    // which we prefer to make a user responsibility.
                    adapter = makeUnboxArgument(midType, adapter, i, dst);
                } else {
                    // Simple reference conversion.
                    // Note:  Do not check for a class hierarchy relation
                    // between src and dst.  In all cases a 'null' argument
                    // will pass the cast conversion.
                    adapter = makeCheckCast(midType, adapter, i, dst);
                }
            }
            assert(adapter != null);
            assert(adapter.type() == midType);
        }
        if (adapter.type() != newType) {
            // Only trivial conversions remain.
            adapter = makeRetypeOnly(newType, adapter);
            assert(adapter != null);
            // Actually, that's because there were no non-trivial ones:
            assert(lastConv == -1);
        }
        assert(adapter.type() == newType);
        return adapter;
    }

    /**
     * Create a JVM-level adapter method handle to permute the arguments
     * of the given method.
     * @param newType required call type
     * @param target original method handle
     * @param argumentMap for each target argument, position of its source in newType
     * @return an adapter to the original handle with the desired new type,
     *          or the original target if the types are already identical
     *          and the permutation is null
     * @throws IllegalArgumentException if the adaptation cannot be made
     *          directly by a JVM-level adapter, without help from Java code
     */
    static MethodHandle makePermutation(MethodType newType, MethodHandle target,
                int[] argumentMap) {
        MethodType oldType = target.type();
        boolean nullPermutation = true;
        for (int i = 0; i < argumentMap.length; i++) {
            int pos = argumentMap[i];
            if (pos != i)
                nullPermutation = false;
            if (pos < 0 || pos >= newType.parameterCount()) {
                argumentMap = new int[0]; break;
            }
        }
        if (argumentMap.length != oldType.parameterCount())
            throw newIllegalArgumentException("bad permutation: "+Arrays.toString(argumentMap));
        if (nullPermutation) {
            MethodHandle res = makePairwiseConvert(newType, target);
            // well, that was easy
            if (res == null)
                throw newIllegalArgumentException("cannot convert pairwise: "+newType);
            return res;
        }

        // Check return type.  (Not much can be done with it.)
        Class<?> exp = newType.returnType();
        Class<?> ret = oldType.returnType();
        if (!VerifyType.isNullConversion(ret, exp))
            throw newIllegalArgumentException("bad return conversion for "+newType);

        // See if the argument types match up.
        for (int i = 0; i < argumentMap.length; i++) {
            int j = argumentMap[i];
            Class<?> src = newType.parameterType(j);
            Class<?> dst = oldType.parameterType(i);
            if (!VerifyType.isNullConversion(src, dst))
                throw newIllegalArgumentException("bad argument #"+j+" conversion for "+newType);
        }

        // Now figure out a nice mix of SWAP, ROT, DUP, and DROP adapters.
        // A workable greedy algorithm is as follows:
        // Drop unused outgoing arguments (right to left: shallowest first).
        // Duplicate doubly-used outgoing arguments (left to right: deepest first).
        // Then the remaining problem is a true argument permutation.
        // Marshal the outgoing arguments as required from left to right.
        // That is, find the deepest outgoing stack position that does not yet
        // have the correct argument value, and correct at least that position
        // by swapping or rotating in the misplaced value (from a shallower place).
        // If the misplaced value is followed by one or more consecutive values
        // (also misplaced)  issue a rotation which brings as many as possible
        // into position.  Otherwise make progress with either a swap or a
        // rotation.  Prefer the swap as cheaper, but do not use it if it
        // breaks a slot pair.  Prefer the rotation over the swap if it would
        // preserve more consecutive values shallower than the target position.
        // When more than one rotation will work (because the required value
        // is already adjacent to the target position), then use a rotation
        // which moves the old value in the target position adjacent to
        // one of its consecutive values.  Also, prefer shorter rotation
        // spans, since they use fewer memory cycles for shuffling.

        throw new UnsupportedOperationException("NYI");
    }

    private static byte basicType(Class<?> type) {
        if (type == null)  return T_VOID;
        switch (Wrapper.forBasicType(type)) {
            case BOOLEAN:  return T_BOOLEAN;
            case CHAR:     return T_CHAR;
            case FLOAT:    return T_FLOAT;
            case DOUBLE:   return T_DOUBLE;
            case BYTE:     return T_BYTE;
            case SHORT:    return T_SHORT;
            case INT:      return T_INT;
            case LONG:     return T_LONG;
            case OBJECT:   return T_OBJECT;
            case VOID:     return T_VOID;
        }
        return 99; // T_ILLEGAL or some such
    }

    /** Number of stack slots for the given type.
     *  Two for T_DOUBLE and T_FLOAT, one for the rest.
     */
    private static int type2size(int type) {
        assert(type >= T_BOOLEAN && type <= T_OBJECT);
        return (type == T_LONG || type == T_DOUBLE) ? 2 : 1;
    }
    private static int type2size(Class<?> type) {
        return type2size(basicType(type));
    }

    /** The given stackMove is the number of slots pushed.
     * It might be negative.  Scale it (multiply) by the
     * VM's notion of how an address changes with a push,
     * to get the raw SP change for stackMove.
     * Then shift and mask it into the correct field.
     */
    private static long insertStackMove(int stackMove) {
        // following variable must be long to avoid sign extension after '<<'
        long spChange = stackMove * MethodHandleNatives.JVM_STACK_MOVE_UNIT;
        return (spChange & CONV_STACK_MOVE_MASK) << CONV_STACK_MOVE_SHIFT;
    }

    /** Construct an adapter conversion descriptor for a single-argument conversion. */
    private static long makeConv(int convOp, int argnum, int src, int dest) {
        assert(src  == (src  & 0xF));
        assert(dest == (dest & 0xF));
        assert(convOp >= OP_CHECK_CAST && convOp <= OP_PRIM_TO_REF);
        int stackMove = type2size(dest) - type2size(src);
        return ((long) argnum << 32 |
                (long) convOp << CONV_OP_SHIFT |
                (int)  src    << CONV_SRC_TYPE_SHIFT |
                (int)  dest   << CONV_DEST_TYPE_SHIFT |
                insertStackMove(stackMove)
                );
    }
    private static long makeConv(int convOp, int argnum, int stackMove) {
        assert(convOp >= OP_DUP_ARGS && convOp <= OP_SPREAD_ARGS);
        byte src = 0, dest = 0;
        if (convOp >= OP_COLLECT_ARGS && convOp <= OP_SPREAD_ARGS)
            src = dest = T_OBJECT;
        return ((long) argnum << 32 |
                (long) convOp << CONV_OP_SHIFT |
                (int)  src    << CONV_SRC_TYPE_SHIFT |
                (int)  dest   << CONV_DEST_TYPE_SHIFT |
                insertStackMove(stackMove)
                );
    }
    private static long makeSwapConv(int convOp, int srcArg, byte type, int destSlot) {
        assert(convOp >= OP_SWAP_ARGS && convOp <= OP_ROT_ARGS);
        return ((long) srcArg << 32 |
                (long) convOp << CONV_OP_SHIFT |
                (int)  type   << CONV_SRC_TYPE_SHIFT |
                (int)  type   << CONV_DEST_TYPE_SHIFT |
                (int)  destSlot << CONV_VMINFO_SHIFT
                );
    }
    private static long makeConv(int convOp) {
        assert(convOp == OP_RETYPE_ONLY || convOp == OP_RETYPE_RAW);
        return ((long)-1 << 32) | (convOp << CONV_OP_SHIFT);   // stackMove, src, dst all zero
    }
    private static int convCode(long conv) {
        return (int)conv;
    }
    private static int convArgPos(long conv) {
        return (int)(conv >>> 32);
    }
    private static boolean convOpSupported(int convOp) {
        assert(convOp >= 0 && convOp <= CONV_OP_LIMIT);
        return ((1<<convOp) & MethodHandleNatives.CONV_OP_IMPLEMENTED_MASK) != 0;
    }

    /** One of OP_RETYPE_ONLY, etc. */
    int conversionOp() { return (conversion & CONV_OP_MASK) >> CONV_OP_SHIFT; }

    /* Return one plus the position of the first non-trivial difference
     * between the given types.  This is not a symmetric operation;
     * we are considering adapting the targetType to adapterType.
     * Trivial differences are those which could be ignored by the JVM
     * without subverting the verifier.  Otherwise, adaptable differences
     * are ones for which we could create an adapter to make the type change.
     * Return zero if there are no differences (other than trivial ones).
     * Return 1+N if N is the only adaptable argument difference.
     * Return the -2-N where N is the first of several adaptable
     * argument differences.
     * Return -1 if there there are differences which are not adaptable.
     */
    private static int diffTypes(MethodType adapterType,
                                 MethodType targetType,
                                 boolean raw) {
        int diff;
        diff = diffReturnTypes(adapterType, targetType, raw);
        if (diff != 0)  return diff;
        int nargs = adapterType.parameterCount();
        if (nargs != targetType.parameterCount())
            return -1;
        diff = diffParamTypes(adapterType, 0, targetType, 0, nargs, raw);
        //System.out.println("diff "+adapterType);
        //System.out.println("  "+diff+" "+targetType);
        return diff;
    }
    private static int diffReturnTypes(MethodType adapterType,
                                       MethodType targetType,
                                       boolean raw) {
        Class<?> src = targetType.returnType();
        Class<?> dst = adapterType.returnType();
        if ((!raw
             ? VerifyType.canPassUnchecked(src, dst)
             : VerifyType.canPassRaw(src, dst)
             ) > 0)
            return 0;  // no significant difference
        if (raw && !src.isPrimitive() && !dst.isPrimitive())
            return 0;  // can force a reference return (very carefully!)
        //if (false)  return 1;  // never adaptable!
        return -1;  // some significant difference
    }
    private static int diffParamTypes(MethodType adapterType, int astart,
                                      MethodType targetType, int tstart,
                                      int nargs, boolean raw) {
        assert(nargs >= 0);
        int res = 0;
        for (int i = 0; i < nargs; i++) {
            Class<?> src  = adapterType.parameterType(astart+i);
            Class<?> dest = targetType.parameterType(tstart+i);
            if ((!raw
                 ? VerifyType.canPassUnchecked(src, dest)
                 : VerifyType.canPassRaw(src, dest)
                ) <= 0) {
                // found a difference; is it the only one so far?
                if (res != 0)
                    return -1-res; // return -2-i for prev. i
                res = 1+i;
            }
        }
        return res;
    }

    /** Can a retyping adapter (alone) validly convert the target to newType? */
    static boolean canRetypeOnly(MethodType newType, MethodType targetType) {
        return canRetype(newType, targetType, false);
    }
    /** Can a retyping adapter (alone) convert the target to newType?
     *  It is allowed to widen subword types and void to int, to make bitwise
     *  conversions between float/int and double/long, and to perform unchecked
     *  reference conversions on return.  This last feature requires that the
     *  caller be trusted, and perform explicit cast conversions on return values.
     */
    static boolean canRetypeRaw(MethodType newType, MethodType targetType) {
        return canRetype(newType, targetType, true);
    }
    static boolean canRetype(MethodType newType, MethodType targetType, boolean raw) {
        if (!convOpSupported(raw ? OP_RETYPE_RAW : OP_RETYPE_ONLY))  return false;
        int diff = diffTypes(newType, targetType, raw);
        // %%% This assert is too strong.  Factor diff into VerifyType and reconcile.
        assert(raw || (diff == 0) == VerifyType.isNullConversion(newType, targetType));
        return diff == 0;
    }

    /** Factory method:  Performs no conversions; simply retypes the adapter.
     *  Allows unchecked argument conversions pairwise, if they are safe.
     *  Returns null if not possible.
     */
    static MethodHandle makeRetypeOnly(MethodType newType, MethodHandle target) {
        return makeRetype(newType, target, false);
    }
    static MethodHandle makeRetypeRaw(MethodType newType, MethodHandle target) {
        return makeRetype(newType, target, true);
    }
    static MethodHandle makeRetype(MethodType newType, MethodHandle target, boolean raw) {
        MethodType oldType = target.type();
        if (oldType == newType)  return target;
        if (!canRetype(newType, oldType, raw))
            return null;
        // TO DO:  clone the target guy, whatever he is, with new type.
        return new AdapterMethodHandle(target, newType, makeConv(raw ? OP_RETYPE_RAW : OP_RETYPE_ONLY));
    }

    static MethodHandle makeVarargsCollector(MethodHandle target, Class<?> arrayType) {
        return new AsVarargsCollector(target, arrayType);
    }

    static class AsVarargsCollector extends AdapterMethodHandle {
        final MethodHandle target;
        final Class<?> arrayType;
        MethodHandle cache;

        AsVarargsCollector(MethodHandle target, Class<?> arrayType) {
            super(target, target.type(), makeConv(OP_RETYPE_ONLY));
            this.target = target;
            this.arrayType = arrayType;
            this.cache = target.asCollector(arrayType, 0);
        }

        @Override
        public boolean isVarargsCollector() {
            return true;
        }

        @Override
        public MethodHandle asType(MethodType newType) {
            MethodType type = this.type();
            int collectArg = type.parameterCount() - 1;
            int newArity = newType.parameterCount();
            if (newArity == collectArg+1 &&
                type.parameterType(collectArg).isAssignableFrom(newType.parameterType(collectArg))) {
                // if arity and trailing parameter are compatible, do normal thing
                return super.asType(newType);
            }
            // check cache
            if (cache.type().parameterCount() == newArity)
                return cache.asType(newType);
            // build and cache a collector
            int arrayLength = newArity - collectArg;
            MethodHandle collector;
            try {
                collector = target.asCollector(arrayType, arrayLength);
            } catch (IllegalArgumentException ex) {
                throw new WrongMethodTypeException("cannot build collector");
            }
            cache = collector;
            return collector.asType(newType);
        }

        @Override
        public MethodHandle asVarargsCollector(Class<?> arrayType) {
            MethodType type = this.type();
            if (type.parameterType(type.parameterCount()-1) == arrayType)
                return this;
            return super.asVarargsCollector(arrayType);
        }
    }

    /** Can a checkcast adapter validly convert the target to newType?
     *  The JVM supports all kind of reference casts, even silly ones.
     */
    static boolean canCheckCast(MethodType newType, MethodType targetType,
                int arg, Class<?> castType) {
        if (!convOpSupported(OP_CHECK_CAST))  return false;
        Class<?> src = newType.parameterType(arg);
        Class<?> dst = targetType.parameterType(arg);
        if (!canCheckCast(src, castType)
                || !VerifyType.isNullConversion(castType, dst))
            return false;
        int diff = diffTypes(newType, targetType, false);
        return (diff == arg+1);  // arg is sole non-trivial diff
    }
    /** Can an primitive conversion adapter validly convert src to dst? */
    static boolean canCheckCast(Class<?> src, Class<?> dst) {
        return (!src.isPrimitive() && !dst.isPrimitive());
    }

    /** Factory method:  Forces a cast at the given argument.
     *  The castType is the target of the cast, and can be any type
     *  with a null conversion to the corresponding target parameter.
     *  Return null if this cannot be done.
     */
    static MethodHandle makeCheckCast(MethodType newType, MethodHandle target,
                int arg, Class<?> castType) {
        if (!canCheckCast(newType, target.type(), arg, castType))
            return null;
        long conv = makeConv(OP_CHECK_CAST, arg, T_OBJECT, T_OBJECT);
        return new AdapterMethodHandle(target, newType, conv, castType);
    }

    /** Can an primitive conversion adapter validly convert the target to newType?
     *  The JVM currently supports all conversions except those between
     *  floating and integral types.
     */
    static boolean canPrimCast(MethodType newType, MethodType targetType,
                int arg, Class<?> convType) {
        if (!convOpSupported(OP_PRIM_TO_PRIM))  return false;
        Class<?> src = newType.parameterType(arg);
        Class<?> dst = targetType.parameterType(arg);
        if (!canPrimCast(src, convType)
                || !VerifyType.isNullConversion(convType, dst))
            return false;
        int diff = diffTypes(newType, targetType, false);
        return (diff == arg+1);  // arg is sole non-trivial diff
    }
    /** Can an primitive conversion adapter validly convert src to dst? */
    static boolean canPrimCast(Class<?> src, Class<?> dst) {
        if (src == dst || !src.isPrimitive() || !dst.isPrimitive()) {
            return false;
        } else if (Wrapper.forPrimitiveType(dst).isFloating()) {
            // both must be floating types
            return Wrapper.forPrimitiveType(src).isFloating();
        } else {
            // both are integral, and all combinations work fine
            assert(Wrapper.forPrimitiveType(src).isIntegral() &&
                   Wrapper.forPrimitiveType(dst).isIntegral());
            return true;
        }
    }

    /** Factory method:  Truncate the given argument with zero or sign extension,
     *  and/or convert between single and doubleword versions of integer or float.
     *  The convType is the target of the conversion, and can be any type
     *  with a null conversion to the corresponding target parameter.
     *  Return null if this cannot be done.
     */
    static MethodHandle makePrimCast(MethodType newType, MethodHandle target,
                int arg, Class<?> convType) {
        MethodType oldType = target.type();
        if (!canPrimCast(newType, oldType, arg, convType))
            return null;
        Class<?> src = newType.parameterType(arg);
        long conv = makeConv(OP_PRIM_TO_PRIM, arg, basicType(src), basicType(convType));
        return new AdapterMethodHandle(target, newType, conv);
    }

    /** Can an unboxing conversion validly convert src to dst?
     *  The JVM currently supports all kinds of casting and unboxing.
     *  The convType is the unboxed type; it can be either a primitive or wrapper.
     */
    static boolean canUnboxArgument(MethodType newType, MethodType targetType,
                int arg, Class<?> convType) {
        if (!convOpSupported(OP_REF_TO_PRIM))  return false;
        Class<?> src = newType.parameterType(arg);
        Class<?> dst = targetType.parameterType(arg);
        Class<?> boxType = Wrapper.asWrapperType(convType);
        convType = Wrapper.asPrimitiveType(convType);
        if (!canCheckCast(src, boxType)
                || boxType == convType
                || !VerifyType.isNullConversion(convType, dst))
            return false;
        int diff = diffTypes(newType, targetType, false);
        return (diff == arg+1);  // arg is sole non-trivial diff
    }
    /** Can an primitive unboxing adapter validly convert src to dst? */
    static boolean canUnboxArgument(Class<?> src, Class<?> dst) {
        return (!src.isPrimitive() && Wrapper.asPrimitiveType(dst).isPrimitive());
    }

    /** Factory method:  Unbox the given argument.
     *  Return null if this cannot be done.
     */
    static MethodHandle makeUnboxArgument(MethodType newType, MethodHandle target,
                int arg, Class<?> convType) {
        MethodType oldType = target.type();
        Class<?> src = newType.parameterType(arg);
        Class<?> dst = oldType.parameterType(arg);
        Class<?> boxType = Wrapper.asWrapperType(convType);
        Class<?> primType = Wrapper.asPrimitiveType(convType);
        if (!canUnboxArgument(newType, oldType, arg, convType))
            return null;
        MethodType castDone = newType;
        if (!VerifyType.isNullConversion(src, boxType))
            castDone = newType.changeParameterType(arg, boxType);
        long conv = makeConv(OP_REF_TO_PRIM, arg, T_OBJECT, basicType(primType));
        MethodHandle adapter = new AdapterMethodHandle(target, castDone, conv, boxType);
        if (castDone == newType)
            return adapter;
        return makeCheckCast(newType, adapter, arg, boxType);
    }

    /** Can an primitive boxing adapter validly convert src to dst? */
    static boolean canBoxArgument(Class<?> src, Class<?> dst) {
        if (!convOpSupported(OP_PRIM_TO_REF))  return false;
        throw new UnsupportedOperationException("NYI");
    }

    /** Factory method:  Unbox the given argument.
     *  Return null if this cannot be done.
     */
    static MethodHandle makeBoxArgument(MethodType newType, MethodHandle target,
                int arg, Class<?> convType) {
        // this is difficult to do in the JVM because it must GC
        return null;
    }

    /** Can an adapter simply drop arguments to convert the target to newType? */
    static boolean canDropArguments(MethodType newType, MethodType targetType,
                int dropArgPos, int dropArgCount) {
        if (dropArgCount == 0)
            return canRetypeOnly(newType, targetType);
        if (!convOpSupported(OP_DROP_ARGS))  return false;
        if (diffReturnTypes(newType, targetType, false) != 0)
            return false;
        int nptypes = newType.parameterCount();
        // parameter types must be the same up to the drop point
        if (dropArgPos != 0 && diffParamTypes(newType, 0, targetType, 0, dropArgPos, false) != 0)
            return false;
        int afterPos = dropArgPos + dropArgCount;
        int afterCount = nptypes - afterPos;
        if (dropArgPos < 0 || dropArgPos >= nptypes ||
            dropArgCount < 1 || afterPos > nptypes ||
            targetType.parameterCount() != nptypes - dropArgCount)
            return false;
        // parameter types after the drop point must also be the same
        if (afterCount != 0 && diffParamTypes(newType, afterPos, targetType, dropArgPos, afterCount, false) != 0)
            return false;
        return true;
    }

    /** Factory method:  Drop selected arguments.
     *  Allow unchecked retyping of remaining arguments, pairwise.
     *  Return null if this is not possible.
     */
    static MethodHandle makeDropArguments(MethodType newType, MethodHandle target,
                int dropArgPos, int dropArgCount) {
        if (dropArgCount == 0)
            return makeRetypeOnly(newType, target);
        if (!canDropArguments(newType, target.type(), dropArgPos, dropArgCount))
            return null;
        // in  arglist: [0: ...keep1 | dpos: drop... | dpos+dcount: keep2... ]
        // out arglist: [0: ...keep1 |                        dpos: keep2... ]
        int keep2InPos  = dropArgPos + dropArgCount;
        int dropSlot    = newType.parameterSlotDepth(keep2InPos);
        int keep1InSlot = newType.parameterSlotDepth(dropArgPos);
        int slotCount   = keep1InSlot - dropSlot;
        assert(slotCount >= dropArgCount);
        assert(target.type().parameterSlotCount() + slotCount == newType.parameterSlotCount());
        long conv = makeConv(OP_DROP_ARGS, dropArgPos + dropArgCount - 1, -slotCount);
        return new AdapterMethodHandle(target, newType, conv);
    }

    /** Can an adapter duplicate an argument to convert the target to newType? */
    static boolean canDupArguments(MethodType newType, MethodType targetType,
                int dupArgPos, int dupArgCount) {
        if (!convOpSupported(OP_DUP_ARGS))  return false;
        if (diffReturnTypes(newType, targetType, false) != 0)
            return false;
        int nptypes = newType.parameterCount();
        if (dupArgCount < 0 || dupArgPos + dupArgCount > nptypes)
            return false;
        if (targetType.parameterCount() != nptypes + dupArgCount)
            return false;
        // parameter types must be the same up to the duplicated arguments
        if (diffParamTypes(newType, 0, targetType, 0, nptypes, false) != 0)
            return false;
        // duplicated types must be, well, duplicates
        if (diffParamTypes(newType, dupArgPos, targetType, nptypes, dupArgCount, false) != 0)
            return false;
        return true;
    }

    /** Factory method:  Duplicate the selected argument.
     *  Return null if this is not possible.
     */
    static MethodHandle makeDupArguments(MethodType newType, MethodHandle target,
                int dupArgPos, int dupArgCount) {
        if (!canDupArguments(newType, target.type(), dupArgPos, dupArgCount))
            return null;
        if (dupArgCount == 0)
            return target;
        // in  arglist: [0: ...keep1 | dpos: dup... | dpos+dcount: keep2... ]
        // out arglist: [0: ...keep1 | dpos: dup... | dpos+dcount: keep2... | dup... ]
        int keep2InPos  = dupArgPos + dupArgCount;
        int dupSlot     = newType.parameterSlotDepth(keep2InPos);
        int keep1InSlot = newType.parameterSlotDepth(dupArgPos);
        int slotCount   = keep1InSlot - dupSlot;
        assert(target.type().parameterSlotCount() - slotCount == newType.parameterSlotCount());
        long conv = makeConv(OP_DUP_ARGS, dupArgPos + dupArgCount - 1, slotCount);
        return new AdapterMethodHandle(target, newType, conv);
    }

    /** Can an adapter swap two arguments to convert the target to newType? */
    static boolean canSwapArguments(MethodType newType, MethodType targetType,
                int swapArg1, int swapArg2) {
        if (!convOpSupported(OP_SWAP_ARGS))  return false;
        if (diffReturnTypes(newType, targetType, false) != 0)
            return false;
        if (swapArg1 >= swapArg2)  return false;  // caller resp
        int nptypes = newType.parameterCount();
        if (targetType.parameterCount() != nptypes)
            return false;
        if (swapArg1 < 0 || swapArg2 >= nptypes)
            return false;
        if (diffParamTypes(newType, 0, targetType, 0, swapArg1, false) != 0)
            return false;
        if (diffParamTypes(newType, swapArg1, targetType, swapArg2, 1, false) != 0)
            return false;
        if (diffParamTypes(newType, swapArg1+1, targetType, swapArg1+1, swapArg2-swapArg1-1, false) != 0)
            return false;
        if (diffParamTypes(newType, swapArg2, targetType, swapArg1, 1, false) != 0)
            return false;
        if (diffParamTypes(newType, swapArg2+1, targetType, swapArg2+1, nptypes-swapArg2-1, false) != 0)
            return false;
        return true;
    }

    /** Factory method:  Swap the selected arguments.
     *  Return null if this is not possible.
     */
    static MethodHandle makeSwapArguments(MethodType newType, MethodHandle target,
                int swapArg1, int swapArg2) {
        if (swapArg1 == swapArg2)
            return target;
        if (swapArg1 > swapArg2) { int t = swapArg1; swapArg1 = swapArg2; swapArg2 = t; }
        if (!canSwapArguments(newType, target.type(), swapArg1, swapArg2))
            return null;
        Class<?> swapType = newType.parameterType(swapArg1);
        // in  arglist: [0: ...keep1 | pos1: a1 | pos1+1: keep2... | pos2: a2 | pos2+1: keep3... ]
        // out arglist: [0: ...keep1 | pos1: a2 | pos1+1: keep2... | pos2: a1 | pos2+1: keep3... ]
        int swapSlot2  = newType.parameterSlotDepth(swapArg2 + 1);
        long conv = makeSwapConv(OP_SWAP_ARGS, swapArg1, basicType(swapType), swapSlot2);
        return new AdapterMethodHandle(target, newType, conv);
    }

    static int positiveRotation(int argCount, int rotateBy) {
        assert(argCount > 0);
        if (rotateBy >= 0) {
            if (rotateBy < argCount)
                return rotateBy;
            return rotateBy % argCount;
        } else if (rotateBy >= -argCount) {
            return rotateBy + argCount;
        } else {
            return (-1-((-1-rotateBy) % argCount)) + argCount;
        }
    }

    final static int MAX_ARG_ROTATION = 1;

    /** Can an adapter rotate arguments to convert the target to newType? */
    static boolean canRotateArguments(MethodType newType, MethodType targetType,
                int firstArg, int argCount, int rotateBy) {
        if (!convOpSupported(OP_ROT_ARGS))  return false;
        if (argCount <= 2)  return false;  // must be a swap, not a rotate
        rotateBy = positiveRotation(argCount, rotateBy);
        if (rotateBy == 0)  return false;  // no rotation
        if (rotateBy > MAX_ARG_ROTATION && rotateBy < argCount - MAX_ARG_ROTATION)
            return false;  // too many argument positions
        // Rotate incoming args right N to the out args, N in 1..(argCouunt-1).
        if (diffReturnTypes(newType, targetType, false) != 0)
            return false;
        int nptypes = newType.parameterCount();
        if (targetType.parameterCount() != nptypes)
            return false;
        if (firstArg < 0 || firstArg >= nptypes)  return false;
        int argLimit = firstArg + argCount;
        if (argLimit > nptypes)  return false;
        if (diffParamTypes(newType, 0, targetType, 0, firstArg, false) != 0)
            return false;
        int newChunk1 = argCount - rotateBy, newChunk2 = rotateBy;
        // swap new chunk1 with target chunk2
        if (diffParamTypes(newType, firstArg, targetType, argLimit-newChunk1, newChunk1, false) != 0)
            return false;
        // swap new chunk2 with target chunk1
        if (diffParamTypes(newType, firstArg+newChunk1, targetType, firstArg, newChunk2, false) != 0)
            return false;
        return true;
    }

    /** Factory method:  Rotate the selected argument range.
     *  Return null if this is not possible.
     */
    static MethodHandle makeRotateArguments(MethodType newType, MethodHandle target,
                int firstArg, int argCount, int rotateBy) {
        rotateBy = positiveRotation(argCount, rotateBy);
        if (!canRotateArguments(newType, target.type(), firstArg, argCount, rotateBy))
            return null;
        // Decide whether it should be done as a right or left rotation,
        // on the JVM stack.  Return the number of stack slots to rotate by,
        // positive if right, negative if left.
        int limit = firstArg + argCount;
        int depth0 = newType.parameterSlotDepth(firstArg);
        int depth1 = newType.parameterSlotDepth(limit-rotateBy);
        int depth2 = newType.parameterSlotDepth(limit);
        int chunk1Slots = depth0 - depth1; assert(chunk1Slots > 0);
        int chunk2Slots = depth1 - depth2; assert(chunk2Slots > 0);
        // From here on out, it assumes a single-argument shift.
        assert(MAX_ARG_ROTATION == 1);
        int srcArg, dstArg;
        byte basicType;
        if (chunk2Slots <= chunk1Slots) {
            // Rotate right/down N (rotateBy = +N, N small, c2 small):
            // in  arglist: [0: ...keep1 | arg1: c1...  | limit-N: c2 | limit: keep2... ]
            // out arglist: [0: ...keep1 | arg1: c2 | arg1+N: c1...   | limit: keep2... ]
            srcArg = limit-1;
            dstArg = firstArg;
            basicType = basicType(newType.parameterType(srcArg));
            assert(chunk2Slots == type2size(basicType));
        } else {
            // Rotate left/up N (rotateBy = -N, N small, c1 small):
            // in  arglist: [0: ...keep1 | arg1: c1 | arg1+N: c2...   | limit: keep2... ]
            // out arglist: [0: ...keep1 | arg1: c2 ... | limit-N: c1 | limit: keep2... ]
            srcArg = firstArg;
            dstArg = limit-1;
            basicType = basicType(newType.parameterType(srcArg));
            assert(chunk1Slots == type2size(basicType));
        }
        int dstSlot = newType.parameterSlotDepth(dstArg + 1);
        long conv = makeSwapConv(OP_ROT_ARGS, srcArg, basicType, dstSlot);
        return new AdapterMethodHandle(target, newType, conv);
    }

    /** Can an adapter spread an argument to convert the target to newType? */
    static boolean canSpreadArguments(MethodType newType, MethodType targetType,
                Class<?> spreadArgType, int spreadArgPos, int spreadArgCount) {
        if (!convOpSupported(OP_SPREAD_ARGS))  return false;
        if (diffReturnTypes(newType, targetType, false) != 0)
            return false;
        int nptypes = newType.parameterCount();
        // parameter types must be the same up to the spread point
        if (spreadArgPos != 0 && diffParamTypes(newType, 0, targetType, 0, spreadArgPos, false) != 0)
            return false;
        int afterPos = spreadArgPos + spreadArgCount;
        int afterCount = nptypes - (spreadArgPos + 1);
        if (spreadArgPos < 0 || spreadArgPos >= nptypes ||
            spreadArgCount < 0 ||
            targetType.parameterCount() != afterPos + afterCount)
            return false;
        // parameter types after the spread point must also be the same
        if (afterCount != 0 && diffParamTypes(newType, spreadArgPos+1, targetType, afterPos, afterCount, false) != 0)
            return false;
        // match the array element type to the spread arg types
        Class<?> rawSpreadArgType = newType.parameterType(spreadArgPos);
        if (rawSpreadArgType != spreadArgType && !canCheckCast(rawSpreadArgType, spreadArgType))
            return false;
        for (int i = 0; i < spreadArgCount; i++) {
            Class<?> src = VerifyType.spreadArgElementType(spreadArgType, i);
            Class<?> dst = targetType.parameterType(spreadArgPos + i);
            if (src == null || !VerifyType.isNullConversion(src, dst))
                return false;
        }
        return true;
    }


    /** Factory method:  Spread selected argument. */
    static MethodHandle makeSpreadArguments(MethodType newType, MethodHandle target,
                Class<?> spreadArgType, int spreadArgPos, int spreadArgCount) {
        MethodType targetType = target.type();
        if (!canSpreadArguments(newType, targetType, spreadArgType, spreadArgPos, spreadArgCount))
            return null;
        // in  arglist: [0: ...keep1 | spos: spreadArg | spos+1:      keep2... ]
        // out arglist: [0: ...keep1 | spos: spread... | spos+scount: keep2... ]
        int keep2OutPos  = spreadArgPos + spreadArgCount;
        int spreadSlot   = targetType.parameterSlotDepth(keep2OutPos);
        int keep1OutSlot = targetType.parameterSlotDepth(spreadArgPos);
        int slotCount    = keep1OutSlot - spreadSlot;
        assert(spreadSlot == newType.parameterSlotDepth(spreadArgPos+1));
        assert(slotCount >= spreadArgCount);
        long conv = makeConv(OP_SPREAD_ARGS, spreadArgPos, slotCount-1);
        MethodHandle res = new AdapterMethodHandle(target, newType, conv, spreadArgType);
        assert(res.type().parameterType(spreadArgPos) == spreadArgType);
        return res;
    }

    // TO DO: makeCollectArguments, makeFlyby, makeRicochet

    @Override
    public String toString() {
        return getNameString(nonAdapter((MethodHandle)vmtarget), this);
    }

    private static MethodHandle nonAdapter(MethodHandle mh) {
        while (mh instanceof AdapterMethodHandle) {
            mh = (MethodHandle) mh.vmtarget;
        }
        return mh;
    }
}
