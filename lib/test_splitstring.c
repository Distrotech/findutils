/* test_splitstring.c -- unit test for splitstring()
   Copyright (C) 2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* config.h must always be included first. */
#include <config.h>

/* system headers. */
#include <stdio.h>
#include <assert.h>

/* gnulib headers would go here. */

/* find headers. */
#include "splitstring.h"

static void
assertEqualFunc(const char *file, int line, const char *label,
		size_t expected, size_t got)
{
  if (expected != got)
    fprintf(stderr, "%s line %d: %s: expected %lu, got %lu\n",
	    file, line, label, (unsigned long)expected, (unsigned long)got);
}
#define ASSERT_EQUAL(expected,got) \
  do{ \
    assertEqualFunc(__FILE__,__LINE__,"ASSERT_EQUAL",expected,got); \
    assert (expected == got); \
  } while (0);


static void
test_empty (void)
{
  size_t len, pos;
  bool result;
  const char *empty = "";

  result = splitstring (empty, ":", true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (0, len);
  result = splitstring (empty, ":", false, &pos, &len);
  assert (!result);
}

static void test_onefield (void)
{
  size_t len, pos;
  bool result;
  const char *input = "aaa";

  result = splitstring (input, ":", true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (3, len);
  result = splitstring (input, ":", false, &pos, &len);
  assert (!result);
}

static void test_not_colon (void)
{
  size_t len, pos;
  bool result;
  const char *separators = "!";
  const char *input = "aa!b";

  result = splitstring (input, separators, true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (2, len);

  result = splitstring (input, separators, false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (3, pos);
  ASSERT_EQUAL (1, len);

  result = splitstring (input, separators, false, &pos, &len);
  assert (!result);
}

static void test_empty_back (void)
{
  size_t len, pos;
  bool result;
  const char *input = "aa:";

  result = splitstring (input, ":", true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (2, len);
  result = splitstring (input, ":", false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (3, pos);
  ASSERT_EQUAL (0, len);
  result = splitstring (input, ":", false, &pos, &len);
  assert (!result);
}

static void test_empty_front (void)
{
  size_t len, pos;
  bool result;
  const char *input = ":aaa";

  result = splitstring (input, ":", true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (0, len);
  result = splitstring (input, ":", false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (1, pos);
  ASSERT_EQUAL (3, len);
  result = splitstring (input, ":", false, &pos, &len);
  assert (!result);
}

static void test_twofields (void)
{
  size_t len, pos;
  bool result;
  const char *input = "aaa:bb";

  result = splitstring (input, ":", true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (3, len);
  result = splitstring (input, ":", false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (4, pos);
  ASSERT_EQUAL (2, len);
  result = splitstring (input, ":", false, &pos, &len);
  assert (!result);
}

static void test_twoseparators (void)
{
  size_t len, pos;
  bool result;
  const char *input = "a:bb!c";

  result = splitstring (input, ":!", true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (1, len);
  result = splitstring (input, ":!", false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (2,  pos);
  ASSERT_EQUAL (2, len);
  result = splitstring (input, ":!", false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (5, pos);
  ASSERT_EQUAL (1, len);
  result = splitstring (input, ":!", false, &pos, &len);
  assert (!result);
}

static void test_consecutive_empty (void)
{
  size_t len, pos;
  bool result;
  const char *input = "a::b";
  const char *separators = ":";

  result = splitstring (input, separators, true, &pos, &len);
  assert (result);
  ASSERT_EQUAL (0, pos);
  ASSERT_EQUAL (1, len);

  result = splitstring (input, separators, false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (2, pos);
  ASSERT_EQUAL (0, len);

  result = splitstring (input, separators, false, &pos, &len);
  assert (result);
  ASSERT_EQUAL (3, pos);
  ASSERT_EQUAL (1, len);

  result = splitstring (input, separators, false, &pos, &len);
  assert (!result);
}

int main (int argc, char *argv[])
{
  test_empty ();
  test_onefield ();
  test_not_colon ();
  test_empty_back ();
  test_empty_front ();
  test_twofields ();
  test_twoseparators ();
  test_consecutive_empty ();
  return 0;
}
