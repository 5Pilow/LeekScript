#include "Test.hpp"

void Test::test_set() {

	header("Set");
	section("Constructor");
	code("<>").equals("<>");
	code("<2, 1, 1>").equals("<1, 2>");
	code("<1.56, -2.312, 7.23>").equals("<-2.312, 1.56, 7.23>");
	code("<'2', '1', '1'>").equals("<'1', '2'>");

	section("Set.operator in ()");
	code("let s = <1, 2> 3 in s").equals("false");
	code("let s = <1, 2> 1 in s").equals("true");

	section("Set.operator ==");
	code("<> == <>").equals("true");
	code("<1, 2, 3> == <1, 2, 3>").equals("true");
	code("<1, 2, 3> == <1, 2, 4>").equals("false");
	code("<'a', 'b', 'c'> == <'a', 'b', 'c'>").equals("true");
	code("<'a', 'b', 'c'> == <1, 2, 3>").equals("false");
	code("<1, 2, [3, ''][0]> == <1, 2, 3>").equals("true");
	code("<1.12, 2.12, [3.12, ''][0]> == <1.12, 2.12, 3.12>").equals("true");

	section("Set.operator <");
	code("<> < <>").equals("false");
	code("<1> < <2>").equals("true");
	code("<2> < <1>").equals("false");
	code("<1, 2, 3> < <1, 2, 4>").equals("true");
	code("<2, 2, 3> < <1, 2, 3>").equals("false");
	code("<1.7, 2, 3.90> < <1.7, 2, 5.6>").equals("true");
	code("<'a', 'b', 'c'> < <'a', 'b', 'd'>").equals("true");
	code("<'a', 2, true> < <'a', 2, false>").equals("false");
	code("<1, 2, 3> < <1, 2, []>").equals("true");
	code("<1, 2.5, 3.5> < <1, 2.5, []>").equals("true");
	code("<1, 2, []> < <1, 2, 3>").equals("false");
	code("<1, 2, []> < <1, 2, 3.5>").equals("false");

	section("Set.contains()");
	code("let s = <1, 2> s.contains(3)").equals("false");
	code("let s = <1, 2> s.contains(1)").equals("true");

	section("Set.clear()");
	code("let s = <1, 2> s.clear() s").equals("<>");

	section("Set.erase()");
	code("let s = <1, 2> s.erase(3)").equals("false");
	code("let s = <1, 2> s.erase(1)").equals("true");

	section("Set.insert()");
	code("let s = <1, 2> s.insert(3) s").equals("<1, 2, 3>");

	section("Set.size()");
	code("<>.size()").equals("0");
	code("<1, 2, 3>.size()").equals("3");
	code("<1.6, 2.1, 3.75, 12.12>.size()").equals("4");
	code("<'éééé', null>.size()").equals("2");
	code("Set.size(<1, 2, 3>)").equals("3");
	code("Set.size(<1.6, 2.1, 3.75, 12.12>)").equals("4");
	code("Set.size(<'éééé', null>)").equals("2");

	section("Set clone()");
	code("let s = <1, 2, 3> [s]").equals("[<1, 2, 3>]");

	// Type changes
	// code("let s = <1, 2> s += 'a' s").equals("<1, 2, 'a'>");
}
