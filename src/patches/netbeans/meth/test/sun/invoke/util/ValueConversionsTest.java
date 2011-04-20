/*
 * Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

package sun.invoke.util;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.io.Serializable;
import java.util.Arrays;
import org.junit.Ignore;
import org.junit.Test;
import static org.junit.Assert.*;

/**
 *
 * @author jrose
 */
public class ValueConversionsTest {

    @Test
    public void testUnbox() throws Throwable {
        System.out.println("unbox");
        for (Wrapper w : Wrapper.values()) {
            System.out.println(w);
            boolean exact = (w == Wrapper.SHORT);  // spot-check exact typing version
            for (int n = -5; n < 10; n++) {
                Object box = w.wrap(n);
                switch (w) {
                    case VOID:   assertEquals(box, null); break;
                    case OBJECT: assertEquals(box.getClass(), Integer.class); break;
                    case SHORT:  assertEquals(box.getClass(), Short.class); break;
                    default:     assertEquals(box.getClass(), w.wrapperType()); break;
                }
                MethodHandle unboxer = ValueConversions.unbox(w.primitiveType(), exact);
                Object expResult = box;
                Object result = null;
                switch (w) {
                    case INT:     result = (int)     unboxer.invokeExact(box); break;
                    case LONG:    result = (long)    unboxer.invokeExact(box); break;
                    case FLOAT:   result = (float)   unboxer.invokeExact(box); break;
                    case DOUBLE:  result = (double)  unboxer.invokeExact(box); break;
                    case CHAR:    result = (char)    unboxer.invokeExact(box); break;
                    case BYTE:    result = (byte)    unboxer.invokeExact(box); break;
                    case SHORT:   result = (short)   unboxer.invokeExact((Short)box); break;
                    case OBJECT:  result = (Object)  unboxer.invokeExact(box); break;
                    case BOOLEAN: result = (boolean) unboxer.invokeExact(box); break;
                    case VOID:    result = null;     unboxer.invokeExact(box); break;
                }
                assertEquals("w="+w+" n="+n+" box="+box, expResult, result);
            }
        }
    }

    @Test
    public void testUnboxRaw() throws Throwable {
        System.out.println("unboxRaw");
        for (Wrapper w : Wrapper.values()) {
            if (w == Wrapper.OBJECT)  continue;  // skip this; no raw form
            boolean exact = (w == Wrapper.SHORT);  // spot-check exact typing version
            System.out.println(w);
            for (int n = -5; n < 10; n++) {
                Object box = w.wrap(n);
                long expResult = w.unwrapRaw(box);
                Object box2 = w.wrapRaw(expResult);
                assertEquals(box, box2);
                MethodHandle unboxer = ValueConversions.unboxRaw(w.primitiveType(), exact);
                long result = -1;
                switch (w) {
                    case INT:     result = (int)  unboxer.invokeExact(box); break;
                    case LONG:    result = (long) unboxer.invokeExact(box); break;
                    case FLOAT:   result = (int)  unboxer.invokeExact(box); break;
                    case DOUBLE:  result = (long) unboxer.invokeExact(box); break;
                    case CHAR:    result = (int)  unboxer.invokeExact(box); break;
                    case BYTE:    result = (int)  unboxer.invokeExact(box); break;
                    case SHORT:   result = (int)  unboxer.invokeExact((Short)box); break;
                    case BOOLEAN: result = (int)  unboxer.invokeExact(box); break;
                    case VOID:    result = (int)  unboxer.invokeExact(box); break;
                }
                assertEquals("w="+w+" n="+n+" box="+box, expResult, result);
            }
        }
    }

    @Test
    public void testBox() throws Throwable {
        System.out.println("box");
        for (Wrapper w : Wrapper.values()) {
            if (w == Wrapper.VOID)  continue;  // skip this; no unboxed form
            boolean exact = (w != Wrapper.SHORT);  // spot-check exact typing version
            System.out.println(w);
            for (int n = -5; n < 10; n++) {
                Object box = w.wrap(n);
                MethodHandle boxer = ValueConversions.box(w.primitiveType(), exact);
                Object expResult = box;
                Object result = null;
                switch (w) {
                    case INT:     result = (Integer)   boxer.invokeExact((int)n); break;
                    case LONG:    result = (Long)      boxer.invokeExact((long)n); break;
                    case FLOAT:   result = (Float)     boxer.invokeExact((float)n); break;
                    case DOUBLE:  result = (Double)    boxer.invokeExact((double)n); break;
                    case CHAR:    result = (Character) boxer.invokeExact((char)n); break;
                    case BYTE:    result = (Byte)      boxer.invokeExact((byte)n); break;
                    case SHORT:   result = (Object)    boxer.invokeExact((short)n); break;
                    case OBJECT:  result = (Object)    boxer.invokeExact((Object)n); break;
                    case BOOLEAN: result = (Boolean)   boxer.invokeExact(n != 0); break;
                }
                assertEquals("w="+w+" n="+n+" box="+box, expResult, result);
            }
        }
    }

    @Test
    public void testBoxRaw() throws Throwable {
        System.out.println("boxRaw");
        for (Wrapper w : Wrapper.values()) {
            if (w == Wrapper.VOID)  continue;  // skip this; no unboxed form
            if (w == Wrapper.OBJECT)  continue;  // skip this; no raw form
            boolean exact = (w != Wrapper.SHORT);  // spot-check exact typing version
            System.out.println(w);
            for (int n = -5; n < 10; n++) {
                Object box = w.wrap(n);
                long   raw = w.unwrapRaw(box);
                Object expResult = box;
                MethodHandle boxer = ValueConversions.boxRaw(w.primitiveType(), exact);
                Object result = null;
                switch (w) {
                case INT:     result = (Integer)   boxer.invokeExact((int)raw); break;
                case LONG:    result = (Long)      boxer.invokeExact(raw); break;
                case FLOAT:   result = (Float)     boxer.invokeExact((int)raw); break;
                case DOUBLE:  result = (Double)    boxer.invokeExact(raw); break;
                case CHAR:    result = (Character) boxer.invokeExact((int)raw); break;
                case BYTE:    result = (Byte)      boxer.invokeExact((int)raw); break;
                case SHORT:   result = (Object)    boxer.invokeExact((int)raw); break;
                case BOOLEAN: result = (Boolean)   boxer.invokeExact((int)raw); break;
                }
                assertEquals("w="+w+" n="+n+" box="+box, expResult, result);
            }
        }
    }

    @Test
    public void testReboxRaw() throws Throwable {
        System.out.println("reboxRaw");
        for (Wrapper w : Wrapper.values()) {
            Wrapper pw = Wrapper.forPrimitiveType(w.rawPrimitiveType());
            if (w == Wrapper.VOID)  continue;  // skip this; no unboxed form
            if (w == Wrapper.OBJECT)  continue;  // skip this; no raw form
            boolean exact = (w != Wrapper.SHORT);  // spot-check exact typing version
            System.out.println(w);
            for (int n = -5; n < 10; n++) {
                Object box = w.wrap(n);
                Object raw = pw.wrap(w.unwrapRaw(box));
                Object expResult = box;
                MethodHandle boxer = ValueConversions.rebox(w.primitiveType(), exact);
                Object result = null;
                switch (w) {
                case INT:     result = (Integer)   boxer.invokeExact(raw); break;
                case LONG:    result = (Long)      boxer.invokeExact(raw); break;
                case FLOAT:   result = (Float)     boxer.invokeExact(raw); break;
                case DOUBLE:  result = (Double)    boxer.invokeExact(raw); break;
                case CHAR:    result = (Character) boxer.invokeExact(raw); break;
                case BYTE:    result = (Byte)      boxer.invokeExact(raw); break;
                case SHORT:   result = (Object)    boxer.invokeExact(raw); break;
                case BOOLEAN: result = (Boolean)   boxer.invokeExact(raw); break;
                }
                assertEquals("w="+w+" n="+n+" box="+box, expResult, result);
            }
        }
    }

    @Test
    public void testCast() throws Throwable {
        System.out.println("cast");
        Class<?>[] types = { Object.class, Serializable.class, String.class, Number.class, Integer.class };
        Object[] objects = { new Object(), Boolean.FALSE,      "hello",      (Long)12L,    (Integer)6    };
        for (Class<?> dst : types) {
            boolean exactInt = (dst == Integer.class);
            MethodHandle caster = ValueConversions.cast(dst, exactInt);
            for (Object obj : objects) {
                Class<?> src = obj.getClass();
                boolean canCast;
                if (dst.isInterface()) {
                    canCast = true;
                } else {
                    canCast = dst.isAssignableFrom(src);
                    assertEquals(canCast, dst.isInstance(obj));
                }
                //System.out.println("obj="+obj+" <: dst="+dst);
                try {
                    Object result;
                    if (!exactInt)
                        result = caster.invokeExact(obj);
                    else
                        result = (Integer) caster.invokeExact(obj);
                    if (canCast)
                        assertEquals(obj, result);
                    else
                        assertEquals("cast should not have succeeded", dst, obj);
                } catch (ClassCastException ex) {
                    if (canCast)
                        throw ex;
                }
            }
        }
    }

    @Test
    public void testIdentity() throws Throwable {
        System.out.println("identity");
        MethodHandle id = ValueConversions.identity();
        Object expResult = "foo";
        Object result = id.invokeExact(expResult);
        // compiler bug:  ValueConversions.identity().invokeExact("bar");
        assertEquals(expResult, result);
    }

    @Test
    public void testVarargsArray() throws Throwable {
        System.out.println("varargsArray");
        for (int nargs = 0; nargs <= 5; nargs++) {
            MethodHandle target = ValueConversions.varargsArray(nargs);
            Object[] args = new Object[nargs];
            for (int i = 0; i < nargs; i++)
                args[i] = "#"+i;
            Object res = target.invokeWithArguments(args);
            assertArrayEquals(args, (Object[])res);
        }
    }

    @Test
    public void testVarargsList() throws Throwable {
        System.out.println("varargsList");
        for (int nargs = 0; nargs <= 5; nargs++) {
            MethodHandle target = ValueConversions.varargsList(nargs);
            Object[] args = new Object[nargs];
            for (int i = 0; i < nargs; i++)
                args[i] = "#"+i;
            Object res = target.invokeWithArguments(args);
            assertEquals(Arrays.asList(args), res);
        }
    }
}