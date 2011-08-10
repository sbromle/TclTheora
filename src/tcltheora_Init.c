/*
 * This file is part of MVTH - the Machine Vision Test Harness.
 *
 * Provide the Tcl package interface for the decoding video
 * frames from Ogg Theora files.
 *
 * Copyright (C) 2011 Samuel P. Bromley <sam@sambromley.com>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License Version 3,
 * as published by the Free Software Foundation.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * (see the file named "COPYING"), and a copy of the GNU Lesser General
 * Public License (see the file named "COPYING.LESSER") along with MVTH.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <tcl.h>
#include <tk.h>

int theora_cmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[]) {
	return TCL_OK;
}

int Tcltheora_Init(Tcl_Interp *interp) {
	/* initialize the stub table interface */
	if (Tcl_InitStubs(interp,"8.1",0)==NULL) {
		return TCL_ERROR;
	}

	/* Create a theora Tcl object type */
	
	/* Create all of the Tcl commands */
	Tcl_CreateObjCommand(interp,"theora",theora_cmd,
			(ClientData)NULL,(Tcl_CmdDeleteProc *)NULL);

	Tcl_VarEval(interp,
			"puts stdout {tcltheora Copyright (C) 2011 Sam Bromley};",
			"puts stdout {This software comes with ABSOLUTELY NO WARRANTY.};",
			"puts stdout {This is free software, and you are welcome to};",
			"puts stdout {redistribute it under certain conditions.};",
			"puts stdout {For details, see the GNU Lesser Public License V.3 <http://www.gnu.org/licenses>.};",
			NULL);
	/* Declare that we provide the mvthimage package */
	Tcl_PkgProvide(interp,"tcltheora","1.0");
	return TCL_OK;
}
