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

#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <tcl.h>
#include <tk.h>
#include <ogg/ogg.h>
#include <theora/theoradec.h>
#include <variable_state.h>

#define TCLTHEORA_HASH_KEY "theora_hash"

enum {TCLTHEORA_MAX_NUM_STREAMS=16};

typedef struct theoraDecode_s {
	th_info mInfo;
	th_comment mComment;
	th_setup_info *mSetup;
	th_dec_ctx *mCtx;
} theoraDecode_t;

typedef struct oggStream_s {
	int mSerial;
	ogg_stream_state mState;
	int stream_type;
	int mPacketCount;
	theoraDecode_t mTheora;
} oggStream;

typedef struct tcltheora_object_s {
	FILE *fp; /* handle to Ogg Theora file */
	ogg_sync_state *sync_state; /* ogg file state */
	int headers_read;
	ogg_page *page; /* ogg file page */
	int num_streams; /* number of allocated streams in this file */
	oggStream *streams[TCLTHEORA_MAX_NUM_STREAMS];
} TclTheoraObject;

void theora_free_resources(TclTheoraObject *tto) {
	int i;
	if (tto==NULL) return;
	if (tto->page!=NULL) ckfree((char*)tto->page);
	tto->page=NULL;
	for (i=0;i<tto->num_streams;i++) {
		ogg_stream_clear(&tto->streams[i]->mState);
		th_decode_free(tto->streams[i]->mTheora.mCtx);
		tto->streams[i]->mTheora.mCtx=NULL;
		th_info_clear(&tto->streams[i]->mTheora.mInfo);
		//th_comment_clear(&tto->streams[i]->mTheora.mComment);
		th_setup_free(tto->streams[i]->mTheora.mSetup);
		ckfree((char*)tto->streams[i]);
		tto->streams[i]=NULL;
	}
	tto->num_streams=0;
	if (tto->sync_state!=NULL) {
		ogg_sync_clear(tto->sync_state);
	}
	FILE *fp=tto->fp;
	memset(tto,0,sizeof(TclTheoraObject));
	tto->fp=fp;
	return;
}

void theora_destroy_func(void *ptr) {
	int i;
	TclTheoraObject *tto=(TclTheoraObject *)ptr;
	if (tto!=NULL) {
		theora_free_resources(tto);
		if (tto->fp!=NULL) fclose(tto->fp);
		tto->fp=NULL;
		ckfree((char*)tto);
	}
	return;
}


/* a type to track all TclTheora objects within an Interpreter */
/* get the pointer to a TclTheoraObject by name */
int getTTOFromObj(Tcl_Interp *interp, Tcl_Obj *CONST name, TclTheoraObject **tto) {
	return getVarFromObjKey(TCLTHEORA_HASH_KEY,interp,name,(void**)tto);
}

/* forward definitions */
int TclTheora_NextFrame_Cmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[]);

static int find_stream_by_serial (TclTheoraObject *tto, int serialno) {
	int i;
	for (i=0;i<tto->num_streams;i++) {
		if (tto->streams[i]->mSerial==serialno) return i;
	}
	return -1;
}

static int print_header_info (th_info *info) {
	fprintf(stdout,"Header info:\n");
	fprintf(stdout,"Stream version: %d.%d.%d\n",
			(int)info->version_major,
			(int)info->version_minor,
			(int)info->version_subminor);
	fprintf(stdout,"Frame size %d x %d\n",
			info->frame_width,
			info->frame_height);
	fprintf(stdout,"Pic size %d x %d\n",
			info->pic_width,
			info->pic_height);
	fprintf(stdout,"Pic offset %+d,%+d\n",
			info->pic_x,
			info->pic_y);
	fprintf(stdout,"Aspect ratio: %d/%d\n",
			info->aspect_numerator,info->aspect_denominator);
	switch (info->colorspace) {
		case TH_CS_UNSPECIFIED:
			fprintf(stdout,"Colorspace: TH_CS_UNSPECIFIED\n");
			break;
		case TH_CS_ITU_REC_470M:
			fprintf(stdout,"Colorspace: TH_CS_ITU_REC_470M\n");
			break;
		case TH_CS_ITU_REC_470BG:
			fprintf(stdout,"Colorspace: TH_CS_ITU_REC_470BG\n");
			break;
		default:
			fprintf(stdout,"Colorspace: UNKNOWN\n");
			break;
	}
	switch (info->pixel_fmt) {
		case TH_PF_420:
			fprintf(stdout,"Pixel Format: TH_PF_420\n");
			break;
		case TH_PF_RSVD:
			fprintf(stdout,"Pixel Format: TH_PF_RSVD\n");
			break;
		case TH_PF_422:
			fprintf(stdout,"Pixel Format: TH_PF_422\n");
			break;
		case TH_PF_444:
			fprintf(stdout,"Pixel Format: TH_PF_444\n");
			break;
		default:
			fprintf(stdout,"Pixel Format: UNKNOWN\n");
			break;
	}
	fprintf(stdout,"Frame rate: %d/%d\n",info->fps_numerator,info->fps_denominator);
	fprintf(stdout,"Target bitrate: %d\n",info->target_bitrate);
	fprintf(stdout,"Quality: %d\n",info->quality);
	fprintf(stdout,"Keyframe Granule shift: %d\n",info->keyframe_granule_shift);
}

static int get_next_page (TclTheoraObject *tto) {
	int ret;
	ogg_sync_state *sync_state=tto->sync_state;
	ogg_page *page=tto->page;
	FILE *fp=tto->fp;
	while (ogg_sync_pageout(sync_state,page)!=1) {
		/* get pointer to ogg internal buffer */
		char *buff=ogg_sync_buffer(sync_state,4096);
		if (buff==NULL) {
			fprintf(stderr,"Got NULL buff from ogg_sync_buffer()\n");
			return -1;
		}
		int bytes=fread(buff,sizeof(char),4096,fp);
		if (bytes==0) {
			fprintf(stderr,"End of file.\n");
			return -1;
		}
		ret=ogg_sync_wrote(sync_state,bytes);
		if (ret!=0) {
			fprintf(stderr,"Error from ogg_sync_wrote()\n");
			return -1;
		}
	}
	return 0;
}

int TclTheora_GetInfo_Cmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	CONST char *subCmds[] = {"frameRate",NULL};
	enum TheoraCmdIx {FrameRateIx,};
	int index;

	TclTheoraObject *tto=NULL;
	Tcl_Obj *result=NULL;

	if (Tcl_GetIndexFromObj(interp,objv[1],subCmds,"sub-command",0,&index)!=TCL_OK)
		return TCL_ERROR;
	switch(index) {
		case FrameRateIx:
			if (objc!=2) {
				Tcl_WrongNumArgs(interp,1,objv,"frameRate");
				return TCL_ERROR;
			}
			tto=(TclTheoraObject *)clientData;
			oggStream *stream=tto->streams[0];
			result=Tcl_NewListObj(0,NULL);
			if (Tcl_ListObjAppendElement(interp,result,Tcl_NewIntObj(stream->mTheora.mInfo.fps_numerator))!=TCL_OK) return TCL_ERROR;
			if (Tcl_ListObjAppendElement(interp,result,Tcl_NewIntObj(stream->mTheora.mInfo.fps_denominator))!=TCL_OK) return TCL_ERROR;
			Tcl_SetObjResult(interp,result);
			return TCL_OK;
			break;
		default:
			Tcl_AppendResult(interp,"Unknown subcommand.\n",NULL);
			return TCL_ERROR;
	}
	return TCL_ERROR;
}

int TclTheora_Rewind_Cmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	TclTheoraObject *tto=NULL;
	int i;
	tto=(TclTheoraObject *)clientData;
	if (tto->fp==NULL) {
		Tcl_AppendResult(interp,"Theora Object File not Open.\n",NULL);
		return TCL_ERROR;
	}
	rewind(tto->fp);
	theora_free_resources(tto);
	/* re-initialize things */
	if (initialize_theora_stream(interp,tto)!=TCL_OK) {
		return TCL_ERROR;
	}
	return TCL_OK;
}

int handle_tto_cmd (ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	CONST char *subCmds[] = {"next","frameRate","rewind",NULL};
	enum TheoraCmdIx {NextIx,FrameRateIx,RewindIx};
	int index;

	if (Tcl_GetIndexFromObj(interp,objv[1],subCmds,"sub-command",0,&index)!=TCL_OK)
		return TCL_ERROR;

	switch (index) {
		case NextIx:
			if (objc!=3) {
				Tcl_WrongNumArgs(interp,1,objv,"next photo");
				return TCL_ERROR;
			}
			return TclTheora_NextFrame_Cmd(clientData,interp,objc-1,objv+1);
			break;
		case FrameRateIx:
			if (objc!=2) {
				Tcl_WrongNumArgs(interp,1,objv,"frameRate");
				return TCL_ERROR;
			}
			return TclTheora_GetInfo_Cmd(clientData,interp,objc,objv);
			break;
		case RewindIx:
			if (objc!=2) {
				Tcl_WrongNumArgs(interp,1,objv,"rewind");
				return TCL_ERROR;
			}
			return TclTheora_Rewind_Cmd(clientData,interp,objc,objv);
			break;

		default:
			Tcl_AppendResult(interp,"Unknown subcommand.\n",NULL);
			return TCL_ERROR;
	}
	return TCL_OK;
}

static inline ycbcr_to_rgb(th_info *info, th_ycbcr_buffer buffer,
		Tk_PhotoImageBlock *dst)
{
	unsigned int i,j;
	/* Convert each element to RGB */
	unsigned char r,g,b;
	unsigned char y,cr,cb;
	for (j=0;j<info->pic_height;j++) {
		for (i=0;i<info->pic_width;i++) {
			switch (info->pixel_fmt) {
				case TH_PF_420:
					y=buffer[0].data[(j+info->pic_y)*buffer[0].stride+(i+info->pic_x)];
					cb=buffer[1].data[((j+info->pic_y)/2)*buffer[1].stride+(i+info->pic_x)/2];
					cr=buffer[2].data[((j+info->pic_y)/2)*buffer[2].stride+(i+info->pic_x)/2];
					break;
				case TH_PF_RSVD:
					break;
				case TH_PF_422:
					break;
				case TH_PF_444:
					y=buffer[0].data[(j+info->pic_y)*buffer[0].stride+(i+info->pic_x)];
					cb=buffer[1].data[(j+info->pic_y)*buffer[1].stride+(i+info->pic_x)];
					cr=buffer[2].data[(j+info->pic_y)*buffer[2].stride+(i+info->pic_x)];
					break;
				default:
					break;
			}
			double tmp;
			/* red */
			tmp=255.0*(y-16)/219 + 255*0.701*(cr-128)/112;
			if (tmp<0) tmp=0;
			if (tmp>255) tmp=255;
			r=(unsigned char)tmp;
			/* green */
			tmp=255.0*(y-16)/219 - 255*0.886*0.114*(cb-128)/(112*0.587) - 255*0.701*0.299*(cr-128)/(112*0.587);
			if (tmp<0) tmp=0;
			if (tmp>255) tmp=255;
			g=(unsigned char)tmp;
			/* blue */
			tmp=255.0*(y-16)/219+255.0*0.866*(cb-128)/112;
			if (tmp<0) tmp=0;
			if (tmp>255) tmp=255;
			b=(unsigned char)tmp;
			dst->pixelPtr[j*dst->pitch+i*dst->pixelSize+dst->offset[0]]=r;
			dst->pixelPtr[j*dst->pitch+i*dst->pixelSize+dst->offset[1]]=g;
			dst->pixelPtr[j*dst->pitch+i*dst->pixelSize+dst->offset[2]]=b;
			dst->pixelPtr[j*dst->pitch+i*dst->pixelSize+dst->offset[3]]=255;
		}
	}
	return 0;
}

int initialize_theora_stream (Tcl_Interp *interp, TclTheoraObject *tto) {
	int ret;
	char *msg=NULL;

	/*** is the file a theora stream? ***/
	/* prepare the theora objects */
	tto->sync_state=(ogg_sync_state*)ckalloc(sizeof(ogg_sync_state));
	memset(tto->sync_state,0,sizeof(ogg_sync_state));
	tto->page=(ogg_page*)ckalloc(sizeof(ogg_page));
	/* initialize the ogg state */
	ret=ogg_sync_init(tto->sync_state);
	if (ret!=0) {
		msg="Error initializing ogg stream\n";
		goto error;
	}

	/* Get at the first page */
	if (get_next_page(tto)!=0) {
		msg="Could not get a page from Ogg stream.\n";
		goto error;
	}

	oggStream *oggstream=NULL;
	ogg_stream_state stream_state;
	int serial=ogg_page_serialno(tto->page);
	int cur_stream=find_stream_by_serial(tto,serial);
	if (ogg_page_bos(tto->page)) {
		/* we are at the beginning of a stream */
		if (cur_stream==-1) {
			if (tto->num_streams==TCLTHEORA_MAX_NUM_STREAMS) {
				msg="Too many streams in file.\n";
				goto error;
			}
			cur_stream=tto->num_streams;
			fprintf(stderr,"cur_stream=%d\n",cur_stream);
			tto->streams[cur_stream]=(oggStream*)ckalloc(sizeof(oggStream));
			memset(tto->streams[cur_stream],0,sizeof(oggStream));
			fprintf(stderr,"tto->streams[%d]=%p\n",cur_stream,tto->streams[cur_stream]);
			tto->streams[cur_stream]->mSerial=serial;
			ogg_stream_init(&tto->streams[cur_stream]->mState,serial);
			tto->num_streams++;
		}
		/* we are at the beginning of a stream */
		ret=ogg_stream_init(&tto->streams[cur_stream]->mState,serial);
	} else if (cur_stream==-1) {
		fprintf(stderr,"Some error occurred!\n");
		goto error;
	}

	/* from here on, use stream state  within oggstream */
	/* copy the page into the stream */
	ret = ogg_stream_pagein(&tto->streams[cur_stream]->mState,tto->page);
	if (ret!=0) {
		msg="Error in ogg_stream_pagein().\n";
		goto error;
	}
	/* check to see if we have a full packet stored in the frame */
	ogg_packet packet;
	int done=0;
	while (!done) {
	  ret=ogg_stream_packetpeek(&tto->streams[cur_stream]->mState,&packet);
		while (ret!=1) {
			ret = get_next_page(tto);
			if (ret!=0) break;
			/* from here on, use stream state  within oggstream */
			/* copy the page into the stream */
			ret = ogg_stream_pagein(&tto->streams[cur_stream]->mState,tto->page);
			if (ret!=0) {
				msg="Error in ogg_stream_pagein().\n";
				goto error;
			}
	  	ret=ogg_stream_packetpeek(&tto->streams[cur_stream]->mState,&packet);
		}
		if (ret==-1) {
			fprintf(stderr,"Error. Ran out of data!\n");
			break;
		}
		fprintf(stderr,"Got a packet\n");
		tto->streams[cur_stream]->mPacketCount++;
		/* is it a theora packet? */
		int break_early=0;
		if (!tto->headers_read) {
			fprintf(stderr,"calling th_decode_headerin\n");
			ret = th_decode_headerin(
					&tto->streams[cur_stream]->mTheora.mInfo,
					&tto->streams[cur_stream]->mTheora.mComment,
					&tto->streams[cur_stream]->mTheora.mSetup,
					&packet);
			switch (ret) {
				case TH_EFAULT:
					fprintf(stderr,"TH_EFAULT\n");
					break_early=1;
					break;
				case TH_EBADHEADER:
					fprintf(stderr,"TH_EBADHEADER\n");
					break_early=1;
					break;
				case TH_EVERSION:
					fprintf(stderr,"TH_EVERSION\n");
					break_early=1;
					break;
				case TH_ENOTFORMAT:
					fprintf(stderr,"Not TH_ENOTFORMAT, but %d\n",ret);
					break_early=1;
					break;
			}
			if (break_early) {
				/* advance the stream */
	  		ogg_stream_packetout(&tto->streams[cur_stream]->mState,&packet);
				continue;
			}
			/* otherwise it *is* a theora packet, therefore
			 * we have a valid theora stream. We can finish setting everything
			 * up, register a new command, and return.
			 * But first, read any remaining headers.
			 */
			if (ret>0) {
				fprintf(stderr,"ret=%d\n",ret);
				/* then we have a header packet. Might as well get another one */
				/* first advance the packet stream */
	  		ret=ogg_stream_packetout(&tto->streams[cur_stream]->mState,&packet);
				continue;
			}
			done=1;
			tto->headers_read=1;
			fprintf(stderr,"All headers read for stream %d\n",serial);
			/* otherwise, we've found the first video data packet! */
			if (tto->streams[cur_stream]->mTheora.mCtx==NULL) {
				tto->streams[cur_stream]->mTheora.mCtx = th_decode_alloc(
						&tto->streams[cur_stream]->mTheora.mInfo,
						tto->streams[cur_stream]->mTheora.mSetup);
			}
			if (tto->streams[cur_stream]->mTheora.mCtx==NULL) {
				Tcl_AppendResult(interp,"Error allocating Theora Context!\n",NULL);
				goto error;
			}
#if 0
			/* try to decode the data packet */
			/* FIXME: For now, I need to do this here, otherwise we'll loose
			 * the first frame, and the theora decode won't have the first
			 * key-frame for reference. To fix this, I'll need to either
			 * store the first data packet for later, to be used by the first
			 * call to NextFrame. Or I'll have to make things smarter in some
			 * other way. For now, just decode it to make everyone happy. (Well,
			 * everyone except the user who wants to see the first frame!)
			 */
			ogg_int64_t granulepos=-1;
			ret = th_decode_packetin(oggstream->mTheora.mCtx,&packet,&granulepos);
			if (ret==0) {
				/* then all we have to do is decode the frame */
				th_ycbcr_buffer buffer;
				ret = th_decode_ycbcr_out(oggstream->mTheora.mCtx,buffer);
			}
#endif
		} 
	}
	return TCL_OK;

error:
	theora_destroy_func((void*)tto);
	Tcl_AppendResult(interp,msg,NULL);
	return TCL_ERROR;
}

/* command to create a new TclTheora object */
int TclTheora_New_Cmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	FILE *fp=NULL;
	char *msg="Error.\n";
	int ret;
	TclTheoraObject *tto=NULL;

	if (objc!=2) {
		Tcl_WrongNumArgs(interp,1,objv,"ogv_file");
		return TCL_ERROR;
	}

	/* Next, try to open the file */
	fp=fopen(Tcl_GetString(objv[1]),"r");
	if (fp==NULL) {
		Tcl_AppendResult(interp,"Error opening file ",Tcl_GetString(objv[1])," .\n",
				NULL);
		return TCL_ERROR;
	}
	/* ok, make a theora object */
	tto=(TclTheoraObject*)ckalloc(sizeof(TclTheoraObject));
	tto->fp=fp;
	tto->headers_read=0;

	/*** is the file a theora stream? ***/
	/* prepare the theora objects */
	if (initialize_theora_stream(interp,tto)!=TCL_OK) {
		return TCL_ERROR;
	}
	/* if we get here, we have a valid Theora data stream.
	 * Need to create a unique command for operating on this Theora file */
	char cmdname[1024];
	if (varUniqName(interp,(StateManager_t)clientData,cmdname)!=TCL_OK) {
		return TCL_ERROR;
	}
	/* and create the new command */
	Tcl_CreateObjCommand(interp,cmdname,handle_tto_cmd,(ClientData)tto,NULL);

	Tcl_AppendResult(interp,cmdname,NULL);
	return TCL_OK;
}

/* command to grab the next frame from TclTheora object and put it
 * in a tkphoto */
int TclTheora_NextFrame_Cmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	FILE *fp=NULL;
	char *msg="Error.\n";
	int ret;
	TclTheoraObject *tto=NULL;
	ogg_int64_t granulepos=-1;
	th_ycbcr_buffer buffer;

	assert(clientData!=NULL);

	if (objc!=2) {
		Tcl_WrongNumArgs(interp,1,objv,"photo");
		return TCL_ERROR;
	}

	tto=(TclTheoraObject *)clientData;

	/* Get a handle on the photo object */
	Tk_PhotoHandle photo;
	Tk_PhotoImageBlock dst;
	char *str=Tcl_GetString(objv[1]);
	photo = Tk_FindPhoto(interp,str);
	if (photo==NULL) {
		Tcl_AppendResult(interp,"Cannot find photo \"",str,"\"",NULL);
		return TCL_ERROR;
	}

	/* finish reading any packets already in the stream */
	ogg_packet packet;
	oggStream *stream=tto->streams[0];
	th_info *info=&stream->mTheora.mInfo;
	/* FIXME: Only supports first stream for now */
	/* Check to see if a packet is avaliable */
	ret=ogg_stream_packetpeek(&stream->mState,&packet);
	if (ret==0) {
		/* then no packet available, so we need to get a new page */
		if (get_next_page(tto)!=0) {
			/* then we are at the end of the stream */
			/* return 0 */
			Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
			return TCL_OK;
		}
		ret = ogg_stream_pagein(&stream->mState,tto->page);
		if (ret!=0) {
			fprintf(stderr,"%s: Error in ogg_stream_pagein()\n",__func__);
		}
		ret=ogg_stream_packetpeek(&stream->mState,&packet);
	}
	if (ret==1) {
		stream->mPacketCount++;
	}
	/* actually advance the packet */
	ret=ogg_stream_packetout(&stream->mState,&packet);

	/* try to decode the data packet */
	ret = th_decode_packetin(stream->mTheora.mCtx,&packet,&granulepos);
	if (ret==0) {
		/* then all we have to do is decode the frame */
		ret = th_decode_ycbcr_out(stream->mTheora.mCtx,buffer);
		/* ensure the photo is the correct size */
		Tk_PhotoSetSize(interp,photo,info->pic_width,info->pic_height);
		Tk_PhotoGetImage(photo,&dst);

		ycbcr_to_rgb(&stream->mTheora.mInfo,buffer,&dst);
		Tk_PhotoPutBlock(interp,photo,&dst,0,0,info->pic_width,info->pic_height,TK_PHOTO_COMPOSITE_SET);
		/* return 1, since we've recovered a frame */
		Tcl_SetObjResult(interp,Tcl_NewIntObj(1));
		return TCL_OK;
	}

	/* otherwise, we need to get another page and try again */
	int got_another_frame=0;
	int cur_stream=-1;
	while (get_next_page(tto)==0) {
		/* Then we've acquired another page */
		ogg_stream_state stream_state;
		oggStream *newstream;
		int serial=ogg_page_serialno(tto->page);
		cur_stream=find_stream_by_serial(tto,serial);
		if (ogg_page_bos(tto->page)) {
			/* we are at the beginning of a stream */
			if (cur_stream==-1) {
				if (tto->num_streams==TCLTHEORA_MAX_NUM_STREAMS) {
					msg="Too many streams in file.\n";
					return TCL_ERROR;
				}
				tto->streams[tto->num_streams]=(oggStream*)ckalloc(sizeof(oggStream));
				cur_stream=tto->num_streams;
				tto->streams[cur_stream]->mSerial=serial;
				ogg_stream_init(&tto->streams[cur_stream]->mState,serial);
				tto->num_streams++;
			}
			/* we are at the beginning of a stream */
			ret=ogg_stream_init(&tto->streams[cur_stream]->mState,serial);
			/* we only handle the first data stream for now, so just
			 * try to get another page for that stream.*/
			continue;
		} else if (cur_stream==-1) {
			fprintf(stderr,"Some error occured!\n");
			return TCL_ERROR;
			/* FIXME: Hmmm. Maybe the error label below shouldn't free oggstream */
			//stream=find_stream_by_serialno(tto,serial);
			/* we'll just use the "stream" value already set way above */
		}
		/* copy the page into the stream */
		ret = ogg_stream_pagein(&tto->streams[cur_stream]->mState,tto->page);
		if (ret!=0) {
			fprintf(stderr,"Error in ogg_stream_page_in() for stream %d\n",serial);
			fclose(fp);
			return TCL_ERROR;
		}
		/* check to see if we have a full packet stored in the frame */
		ogg_packet packet;
		while (ogg_stream_packetout(&tto->streams[cur_stream]->mState,&packet)!=0) {
			tto->streams[cur_stream]->mPacketCount++;
		}
		/* try to decode the data packet */
		ret = th_decode_packetin(tto->streams[cur_stream]->mTheora.mCtx,&packet,&granulepos);
		if (ret==0) {
			/* then all we have to do is decode the frame */
			ret = th_decode_ycbcr_out(tto->streams[cur_stream]->mTheora.mCtx,buffer);
			ycbcr_to_rgb(&tto->streams[cur_stream]->mTheora.mInfo,buffer,&dst);
			Tk_PhotoPutBlock(interp,photo,&dst,0,0,info->pic_width,info->pic_height,TK_PHOTO_COMPOSITE_SET);
			/* return 1, since we've recovered a frame */
			Tcl_SetObjResult(interp,Tcl_NewIntObj(1));
			return TCL_OK;
		}
	}
	/* return 0, there are no frames left */
	Tcl_SetObjResult(interp,Tcl_NewIntObj(0));
	return TCL_OK;
}

int theora_cmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	CONST char *subCmds[] = {"new",NULL};
	enum TheoraCmdIx {NewIx,};
	int index;

	Tcl_ResetResult(interp);

	if (objc!=3) {
		Tcl_WrongNumArgs(interp,1,objv,"new file");
		return TCL_ERROR;
	}

	if (Tcl_GetIndexFromObj(interp,objv[1],subCmds,"sub-command",0,&index)!=TCL_OK)
		return TCL_ERROR;

	switch (index) {
		case NewIx:
			return TclTheora_New_Cmd(clientData,interp,objc-1,objv+1);
			break;
		default:
			Tcl_AppendResult(interp,"Unknown subcommand.\n",NULL);
			return TCL_ERROR;
	}
}

int Tcltheora_Init(Tcl_Interp *interp) {
	/* initialize the stub table interface */
	if (Tcl_InitStubs(interp,"8.1",0)==NULL) {
		return TCL_ERROR;
	}

	Tcl_VarEval(interp,
			"puts stdout {tcltheora Copyright (C) 2011 Sam Bromley};",
			"puts stdout {This software comes with ABSOLUTELY NO WARRANTY.};",
			"puts stdout {This is free software, and you are welcome to};",
			"puts stdout {redistribute it under certain conditions.};",
			"puts stdout {For details, see the GNU Lesser Public License V.3 <http://www.gnu.org/licenses>.};",
			NULL);

	/* Initialize the variable state manager */
	InitializeStateManager(interp,TCLTHEORA_HASH_KEY,"theora",theora_cmd,theora_destroy_func);
	/* Declare that we provide the tcltheora package */
	Tcl_PkgProvide(interp,"tcltheora","1.0");
	return TCL_OK;
}
