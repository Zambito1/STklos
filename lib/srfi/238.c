/*
 * 238.c   -- Support for SRFI-238 (Codesets)
 *
 * Copyright © 2023 Erick Gallesio <eg@stklos.net>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 *          Authors: Erick Gallesio
 *    Creation date: 22-Jan-2023 09:36
 * Last file update: 24-Jan-2023 12:58 (eg)
 */

#include "stklos.h"
#include "238-incl.c"
#include <string.h>

static int tc_codeset;
static SCM all_codesets= STk_nil; //FIXME: Use a mutex to acess it

//
// New Scheme type codeset
// NOTE: codes and messages are represented by A-lists.
//
struct codeset_obj {
  stk_header header;
  SCM name;
  SCM symbols;
  SCM messages;
};

#define CODESETP(p)           (BOXED_TYPE_EQ((p), tc_codeset))
#define CODESET_NAME(p)       (((struct codeset_obj *) (p))->name)
#define CODESET_SYMBOLS(p)    (((struct codeset_obj *) (p))->symbols)
#define CODESET_MESSAGES(p)   (((struct codeset_obj *) (p))->messages)

static void print_codeset(SCM codeset, SCM port, int _UNUSED(mode))
{
  STk_fprintf(port, "#[codeset-object %s]", SYMBOL_PNAME(CODESET_NAME(codeset)));
}

static struct extended_type_descr xtype_codeset = {
  .name = "codeset-object",
  .print = print_codeset
};

/* ======================================================================
 *
 * Utilities
 *
 * ======================================================================
 */
static void error_bad_codeset_element(SCM obj) {
  STk_error("bad codeset element ~S", obj);
}

static void register_codeset(SCM cs)    // FIXME: MUTEX
{
  all_codesets = STk_cons(STk_cons(CODESET_NAME(cs), cs), all_codesets);
}


// Build a codeset, given a correspondence table and a function to access
// the messages in C. This is used to build the errno and signal codesets.
//
// Since the C functions strerror and strsignal are not thread safe, we cache
// the messages in the codeset_object. Note that even if a reentrant version of
// strerror exists (strerror_r), this is not the case for strsignal. Furthermore,
// these function incur a non negligible time penalty.

static void make_C_codeset(SCM name, struct codeset_code *table,
                           char* (*get_message)(int))
{
  SCM z;
  SCM slist = STk_nil;   /* list of symbols */
  SCM mlist = STk_nil;   /* list of messages */

  //FIXME: MUTEX
  for (struct codeset_code *p=table; p->name; p++) {
    SCM n     = MAKE_INT(p->code);
    char *msg = get_message(p->code);

    /* populate the symbol list */
    slist = STk_cons(STk_cons(n, STk_intern((char *) p->name)), slist);

    /* populate the message list */
    mlist = STk_cons(STk_cons(n, STk_Cstring2string(msg)), mlist);
  }

  NEWCELL(z, codeset);
  CODESET_NAME(z)     = name;
  CODESET_SYMBOLS(z)  = slist;
  CODESET_MESSAGES(z) = mlist;

  register_codeset(z);
}

/* ======================================================================
 *
 * Primitives
 *
 * ======================================================================
 */

/* This procedure is not in SRFI 238. */
DEFINE_PRIMITIVE("codeset-list", codeset_list,
                 subr0, (void))
{ //FIXME: MUTEX
  SCM res = STk_nil;

  for (SCM l = all_codesets; !NULLP(l); l = CDR(l))
    res = STk_cons(CAR(CAR(l)), res);
  return res;
}


DEFINE_PRIMITIVE("codeset?", codesetp, subr1, (SCM obj))
{
  return CODESETP(obj) ?
    STk_true:
    MAKE_BOOLEAN(STk_assq(obj, all_codesets) != STk_false);
}


DEFINE_PRIMITIVE("%find-codeset", find_codeset, subr1, (SCM obj))
{
  if (CODESETP(obj))
    return obj;
  else {
    SCM res = STk_assq(obj, all_codesets);

    if (res == STk_false)
      STk_error("bad codset ~s", obj);
    return CDR(res);
  }
}


DEFINE_PRIMITIVE("%codeset-symbols", codeset_symbols, subr1, (SCM cs))
{
  if (!CODESETP(cs)) STk_error("bad codeset ~s", cs); // already verified in Scheme
  return CODESET_SYMBOLS(cs);
}


DEFINE_PRIMITIVE("%codeset-messages", codeset_messages, subr1, (SCM cs))
{
  if (!CODESETP(cs)) STk_error("bad codeset ~s", cs); // already verified in Scheme
  return CODESET_MESSAGES(cs);
}


DEFINE_PRIMITIVE("%make-user-codeset", make_user_codeset, subr3,
                 (SCM name, SCM codes, SCM messages))
{
  SCM z;

  NEWCELL(z, codeset);
  CODESET_NAME(z) = name;
  CODESET_SYMBOLS(z)  = codes;
  CODESET_MESSAGES(z) = messages;
  register_codeset(z);

  return z;
}

// ======================================================================
MODULE_ENTRY_START("srfi/238")
{
  SCM module =  STk_create_module(STk_intern("srfi/238"));

  tc_codeset = STk_new_user_type(&xtype_codeset);

  ADD_PRIMITIVE_IN_MODULE(codeset_list, module);
  ADD_PRIMITIVE_IN_MODULE(codesetp, module);
  ADD_PRIMITIVE_IN_MODULE(find_codeset, module);
  ADD_PRIMITIVE_IN_MODULE(codeset_symbols,module);
  ADD_PRIMITIVE_IN_MODULE(codeset_messages,module);
  ADD_PRIMITIVE_IN_MODULE(make_user_codeset, module);

  /* Create the errno & signal codesets */
  make_C_codeset(STk_intern("errno"),  STk_errno_names,  strerror);
  make_C_codeset(STk_intern("signal"), STk_signal_names, strsignal);

  /* Execute Scheme code */
  STk_execute_C_bytecode(__module_consts, __module_code);
}
MODULE_ENTRY_END

DEFINE_MODULE_INFO
