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

import java.util.Arrays;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import static org.junit.Assert.*;

/**
 *
 * @author jrose
 */
public class BytecodeNameTest {

    public BytecodeNameTest() {
    }

    @BeforeClass
    public static void setUpClass() throws Exception {
    }

    @AfterClass
    public static void tearDownClass() throws Exception {
    }

    static String[][] SAMPLES = {
        // mangled, source
        {"foo", "foo"},
        {"ba\\r", "ba\\r"},
        {"\\=ba\\-%z", "ba\\%z"},
        {"\\=ba\\--z", "ba\\-z"},
        {"=\\=", "=\\="},
        {"\\==\\|\\=", "=/\\="},
        {"\\|\\=", "/\\="},
        {"\\=ba\\!", "ba:"},
        {"\\|", "/"},
        {"\\", "\\"},
        {"\\\\%", "\\$"},
        {"\\\\", "\\\\"},
        {"\\=", ""}
    };

    static String[][] canonicalSamples() {
        int ndc = DANGEROUS_CHARS.length();
        String[][] res = new String[2*ndc][];
        for (int i = 0; i < ndc; i++) {
            char dc = DANGEROUS_CHARS.charAt(i);
            char rc = REPLACEMENT_CHARS.charAt(i);
            if (dc == '\\') {
                res[2*i+0] = new String[] { "\\-%", "\\%" };
            } else {
                res[2*i+0] = new String[] { "\\"+rc, ""+dc };
            }
            res[2*i+1] = new String[] { ""+rc, ""+rc };
        }
        return res;
    }

    @Test
    public void testToBytecodeName() {
        System.out.println("toBytecodeName");
        testToBytecodeName(SAMPLES);
        testToBytecodeName(canonicalSamples());
    }
    public void testToBytecodeName(String[][] samples) {
        for (String[] sample : samples) {
            String s = sample[1];
            String expResult = sample[0];
            String result = BytecodeName.toBytecodeName(s);
            if (!result.equals(expResult))
                System.out.println(s+" => "+result+" != "+expResult);
            assertEquals(expResult, result);
        }
    }

    @Test
    public void testToSourceName() {
        System.out.println("toSourceName");
        testToBytecodeName(SAMPLES);
        testToBytecodeName(canonicalSamples());
    }
    public void testToSourceName(String[][] samples) {
        for (String[] sample : samples) {
            String s = sample[0];
            String expResult = sample[1];
            String result = BytecodeName.toBytecodeName(s);
            assertEquals(expResult, result);
        }
    }

    Object[][] PARSE_SAMPLES = {
        {""},
        {"/", '/'},
        {"\\", "\\"},
        {":foo", ':', "foo"},
        {"\\=ba\\!", "ba:"},
        {"java/lang/String", "java", '/', "lang", '/', "String"},
        {"<init>", '<', "init", '>'},
        {"foo/bar$:baz", "foo", '/', "bar", '$', ':', "baz"},
        {"::\\=:foo:\\=bar\\!baz", ':', ':', "", ':', "foo", ':', "bar:baz"},
    };

    @Test
    public void testParseBytecodeName() {
        System.out.println("parseBytecodeName");
        for (Object[] sample : PARSE_SAMPLES)
            testParseBytecodeName((String) sample[0], Arrays.copyOfRange(sample, 1, sample.length));
        for (String[] sample : SAMPLES) {
            for (char dc : DANGEROUS_CHARS.toCharArray()) {
                if (dc == '\\')  continue;
                testParseBytecodeName(sample[0], sample[1]);
                testParseBytecodeName(dc+sample[0], dc, sample[1]);
                testParseBytecodeName(dc+sample[0]+dc, dc, sample[1], dc);
                testParseBytecodeName(sample[0]+dc, sample[1], dc);
            }
        }
    }
    public void testParseBytecodeName(String s, Object... expResult) {
        Object[] result = BytecodeName.parseBytecodeName(s);
        if (!Arrays.equals(expResult, result))
            System.out.println(s+" => "+Arrays.toString(result)+" != "+Arrays.toString(expResult));
        assertArrayEquals(expResult, result);
    }

    @Test
    public void testUnparseBytecodeName() {
        System.out.println("unparseBytecodeName");
        for (Object[] sample : PARSE_SAMPLES)
            testUnparseBytecodeName((String) sample[0], Arrays.copyOfRange(sample, 1, sample.length));
        for (String[] sample : SAMPLES) {
            for (char dc : DANGEROUS_CHARS.toCharArray()) {
                if (dc == '\\')  continue;
                testUnparseBytecodeName(sample[0], sample[1]);
                testUnparseBytecodeName(dc+sample[0], dc, sample[1]);
                testUnparseBytecodeName(dc+sample[0]+dc, dc, sample[1], dc);
                testUnparseBytecodeName(sample[0]+dc, sample[1], dc);
            }
        }
    }
    public void testUnparseBytecodeName(String expResult, Object... components) {
        String result = BytecodeName.unparseBytecodeName(components);
        assertEquals(expResult, result);
    }

    String[][] DISPLAY_SAMPLES = {
        {"foo", "foo"},
        {"", "" }, // not "''"
        {"\\=", "''" }, // not ""
        {"\\=,\\%,\\=", "',$,\\\\='" },
        {"\\=.\\%.\\=", "''.'$'.''" },
        {"123", "'123'"},
        {"'", "'\\''"},
        {"\\", "'\\\\'"},
        {"java.lang", "java.lang"},
        {"java.123", "java.'123'"},
        {"123.lang", "'123'.lang"},
        {"\\|", "'/'"},
        {"foo:bar", "foo:bar"},
        {"\\=foo\\!bar", "'foo:bar'"},
        {"foo$bar", "foo$bar"},
        {"\\=foo\\%bar", "'foo$bar'"},
    };

    @Test
    public void testToDisplayName() {
        System.out.println("toDisplayName");
        for (String[] sample : DISPLAY_SAMPLES) {
            String s = sample[0];
            String expResult = sample[1];
            String result = BytecodeName.toDisplayName(s);
            if (!result.equals(expResult))
                System.out.println(s+" => "+result+" != "+expResult);
            assertEquals(expResult, result);
        }
    }

    @Test
    public void testIsSafeBytecodeName() {
        System.out.println("isSafeBytecodeName");
        String[] strings = {
            // Safe strings:
            "foo",
            "\\",
            "\\foo",
            "\\=foo",
            "foo\\",
            "foo\\%",
            "fo\\o",
            "fo\\%o",
            "123",
            // Unsafe strings start with "":
            "",
            "foo:bar",
            "<init>",
            "/",
        };
        boolean expResult = true;
        for (String s : strings) {
            if (s.equals(""))  expResult = false;
            boolean result = BytecodeName.isSafeBytecodeName(s);
            assertEquals(expResult, result);
        }
    }

    @Test
    public void testIsSafeBytecodeChar() {
        System.out.println("isSafeBytecodeChar");
        for (char c = 0; c < 500; c++) {
            boolean expResult = c == '\\' || DANGEROUS_CHARS.indexOf(c) < 0;
            boolean result = BytecodeName.isSafeBytecodeChar(c);
            assertEquals(expResult, result);
        }
    }

    // optional test driver
    static void main(String[] av) {
        // If verbose is enabled, quietly check everything.
        // Otherwise, print the output for the user to check.
        boolean verbose = false;

        int maxlen = 0;

        while (av.length > 0 && av[0].startsWith("-")) {
            String flag = av[0].intern();
            av = java.util.Arrays.copyOfRange(av, 1, av.length); // Java 1.6 or later
            if (flag == "-" || flag == "--")  break;
            else if (flag == "-q")
                verbose = false;
            else if (flag == "-v")
                verbose = true;
            else if (flag.startsWith("-l"))
                maxlen = Integer.valueOf(flag.substring(2));
            else
                throw new Error("Illegal flag argument: "+flag);
        }

        if (maxlen == 0)
            maxlen = (verbose ? 2 : 4);
        if (verbose)  System.out.println("Note: maxlen = "+maxlen);

        switch (av.length) {
        case 0: av = new String[] {
                    DANGEROUS_CHARS.substring(0) +
                    REPLACEMENT_CHARS.substring(0, 2) +
                    NULL_ESCAPE + "x"
                }; // and fall through:
        case 1:
            char[] cv = av[0].toCharArray();
            av = new String[cv.length];
            int avp = 0;
            for (char c : cv) {
                String s = String.valueOf(c);
                if (c == 'x')  s = "foo";  // tradition...
                av[avp++] = s;
            }
        }
        if (verbose)
            System.out.println("Note: Verbose output mode enabled.  Use '-q' to suppress.");
        Tester t = new Tester();
        t.maxlen = maxlen;
        t.verbose = verbose;
        t.tokens = av;
        t.test("", 0);
    }

    static char ESCAPE_C = '\\';
    // empty escape sequence to avoid a null name or illegal prefix
    static char NULL_ESCAPE_C = '=';
    static String NULL_ESCAPE = ESCAPE_C+""+NULL_ESCAPE_C;

    static final String DANGEROUS_CHARS   = "\\/.;:$[]<>";
    static final String REPLACEMENT_CHARS =  "-|,?!%{}^_";

    @Test
    public void testMangler() {
        System.out.println("(mangler)");
        Tester t = new Tester();
        t.maxlen = 4;
        String alphabet =
            DANGEROUS_CHARS.substring(0) +
            REPLACEMENT_CHARS.substring(0, 2) +
            NULL_ESCAPE + "x";
        t.tokens = new String[alphabet.length()];
        int fill = 0;
        for (char c : alphabet.toCharArray())
            t.tokens[fill++] = String.valueOf(c);
        t.test("", 0);
        System.out.println("tested mangler exhaustively on "+t.map.size()+" strings to length "+t.maxlen);
    }

    static class Tester {
        boolean verbose;
        int maxlen;
        java.util.Map<String,String> map = new java.util.HashMap<String,String>();
        String[] tokens;

        void test(String stringSoFar, int tokensSoFar) {
            test(stringSoFar);
            if (tokensSoFar <= maxlen) {
                for (String token : tokens) {
                    if (token.length() == 0)  continue;  // skip empty tokens
                    if (stringSoFar.indexOf(token) != stringSoFar.lastIndexOf(token))
                        continue;   // there are already two occs. of this token
                    if (token.charAt(0) == ESCAPE_C && token.length() == 1 && maxlen < 4)
                        test(stringSoFar+token, tokensSoFar);  // want lots of \'s
                    else if (tokensSoFar < maxlen)
                        test(stringSoFar+token, tokensSoFar+1);
                }
            }
        }

        void test(String s) {
            // for small batches, do not test the null string
            if (s.length() == 0 && maxlen >=1 && maxlen <= 2)  return;
            String bn = testSourceName(s);
            if (bn == null)  return;
            if (bn == s) {
                //if (verbose)  System.out.println(s+" == id");
            } else {
                if (verbose)  System.out.println(s+" => "+bn+" "+BytecodeName.toDisplayName(bn));
                String bnbn = testSourceName(bn);
                if (bnbn == null)  return;
                if (verbose)  System.out.println(bn+" => "+bnbn+" "+BytecodeName.toDisplayName(bnbn));
                /*
                String bn3 = testSourceName(bnbn);
                if (bn3 == null)  return;
                if (verbose)  System.out.println(bnbn+" => "+bn3);
                */
            }
        }

        String testSourceName(String s) {
            if (map.containsKey(s))  return null;
            String bn = BytecodeName.toBytecodeName(s);
            map.put(s, bn);
            String sn = BytecodeName.toSourceName(bn);
            if (!sn.equals(s)) {
                String bad = (s+" => "+bn+" != "+sn);
                if (!verbose)  throw new Error("Bad mangling: "+bad);
                System.out.println("*** "+bad);
                return null;
            }
            return bn;
        }
    }
}