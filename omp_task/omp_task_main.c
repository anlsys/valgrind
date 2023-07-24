/*--------------------------------------------------------------------*/
/*--- OpenMP Tasks: a debugger for dependent tasks order of execution */
/*--------------------------------------------------------------------*/

/*
   This file is part of Nulgrind, the minimal Valgrind tool,
   which does no instrumentation or analysis.

   Copyright (C) 2002-2017 Nicholas Nethercote
      njn@valgrind.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h" /* tool_panic */

#if 1
# define OMP_DEBUG(...) VG_(printf)(__VA_ARGS__)
#else
# define OMP_DEBUG(...)
#endif

static void
omp_task_post_clo_init(void)
{
}

static void
omp_task_get_obj_node(IRSB * irsb, IRStmt * st)
{
    static const HChar * anonymous = "???";

    Addr addr;
    DiEpoch ep;
    HChar * file;
    HChar * dir;
    HChar * fn;
    UInt * line;

    addr = st->Ist.IMark.addr + st->Ist.IMark.delta;
    ep = VG_(current_DiEpoch)();
    if (!VG_(get_filename_linenum)(ep, addr, &file, &dir, &line))
    {
        dir  = anonymous;
        file = anonymous;
        line = 0;
    }
    if (!VG_(get_fnname)(ep, addr, &fn))
        fn = anonymous;

    OMP_DEBUG("%s/%s:%d %s\n", dir, file, line, fn);

    // TODO: voir CLG_(get_fn_node) - callgrind/fn.c
}

static IRSB *
omp_task_instrument(
    VgCallbackClosure * closure,
    IRSB * sb_in,
    const VexGuestLayout * layout,
    const VexGuestExtents * vge,
    const VexArchInfo * archinfo_host,
    IRType gWordTy,
    IRType hWordTy
) {
    IRSB * sb_out;
    IRStmt * st;
    Int i;

    if (gWordTy != hWordTy)
        VG_(tool_panic)("host/guest word size mismatch");

    sb_out = deepCopyIRSBExceptStmts(sb_in);

    for (i = 0 ; i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark ; ++i)
        addStmtToIRSB(sb_out, sb_in->stmts[i]);

    omp_task_get_obj_node(sb_in, sb_in->stmts[i]);

    for (i = 0 ; i < sb_in->stmts_used; ++i)
    {
        st = sb_in->stmts[i];
        switch (st->tag)
        {
            case Ist_Store:
            {
                break ;
            }

            default:
            {
                break ;
            }
        }
        addStmtToIRSB(sb_out, sb_in->stmts[i]);
    }

    return sb_out;
}

static void
omp_task_fini(Int exitcode)
{
}

static void
omp_task_pre_clo_init(void)
{
   VG_(details_name)            ("OpenMP Task");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a debugger for dependent tasks order of execution");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2017, and GNU GPL'd, by Romain Pereira.");
   VG_(details_bug_reports_to)  ("romain.pereira@outlook.com");

   VG_(details_avg_translation_sizeB) ( 275 ); // TODO: adjust this

   VG_(basic_tool_funcs)        (omp_task_post_clo_init,
                                 omp_task_instrument,
                                 omp_task_fini);

   /* No needs, no core events to track */
}

VG_DETERMINE_INTERFACE_VERSION(omp_task_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
