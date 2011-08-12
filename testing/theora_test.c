#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <theora/theoradec.h>
#include <glib.h>
#include <assert.h>

enum {TYPE_UNKNOWN = 0,
	TYPE_THEORA=1
};

typedef struct theoraDecode {
	th_info mInfo;
	th_comment mComment;
	th_setup_info *mSetup;
	th_dec_ctx *mCtx;
} theoraDecode;

typedef struct oggstream {
	int mSerial;
	ogg_stream_state mState;
	int stream_type;
	int headers_read;
	int mPacketCount;
	theoraDecode mTheora;
} oggstream;

int print_header_info (th_info *info) {
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

/* The following only works for 444 packing */
int save_ppm_image(th_info *info, th_ycbcr_buffer buffer, int num) {
	FILE *fp=NULL;
	char fname[1024];
	unsigned int i,j;
	sprintf(fname,"frame%05d.ppm",num);
	fp=fopen(fname,"w");
	fprintf(fp,"P6\n%d %d\n255\n",info->pic_width,info->pic_height);
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
			fprintf(fp,"%c%c%c",r,g,b);
		}
	}
	fclose(fp);
	return 0;
}

int get_next_page (ogg_sync_state *state, ogg_page *page, FILE *fp) {
	int ret;
	while (ogg_sync_pageout(state,page)!=1) {
		/* get pointer to ogg internal buffer */
		char *buff=ogg_sync_buffer(state,4096);
		if (buff==NULL) {
			fprintf(stderr,"Got NULL buff from ogg_sync_buffer()\n");
			return -1;
		}
		/* read from the file into the internal buffer */
		int bytes=fread(buff,sizeof(char),4096,fp);
		if (bytes==0) {
			fprintf(stderr,"End of file.\n");
			return -1;
		}
		ret=ogg_sync_wrote(state,bytes);
		if (ret!=0) {
			fprintf(stderr,"Error from ogg_sync_wrote()\n");
			return -1;
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {
	FILE *fp=NULL;
	int ret=0;
	th_ycbcr_buffer buffer;
	ogg_int64_t granulepos=-1;
	/* a hash table to map ids to each Ogg stream */
	GHashTable *hash=g_hash_table_new(g_direct_hash,NULL);

	ogg_sync_state state;
	ogg_page page;

	/* try to open the file */
	if (argc!=2) {
		fprintf(stderr,"Usage: %s oggfile\n",argv[0]);
		return EXIT_FAILURE;
	}
	fp=fopen(argv[1],"r");
	if (fp==NULL) {
		fprintf(stderr,"Error opening file %s\n",argv[1]);
		return EXIT_FAILURE;
	}

	memset(&state,0,sizeof(state));
	memset(&page,0,sizeof(page));

	/* initialize the state tracker */
	ret=ogg_sync_init(&state);
	if (ret!=0) {
		fprintf(stderr,"Error %d in ogg_sync_init()\n",ret);
		return EXIT_FAILURE;
	}

	oggstream *stream=NULL;
	int serial =0;
	/* read all the pages in the file */
	while (get_next_page(&state,&page,fp)==0) {
		serial = ogg_page_serialno(&page);
		stream=NULL;
		if (ogg_page_bos(&page)) {
			/* then we are at a bos (begining of a stream) */
			stream=(oggstream*)calloc(1,sizeof(oggstream));
			ret=ogg_stream_init(&stream->mState,serial);
			stream->mSerial=serial;
			fprintf(stderr,"Inserting serial %d into hash\n",serial);
			g_hash_table_insert(hash,GINT_TO_POINTER(serial),stream);
		} else {
			/* retrieve the previously stored stream */
			stream=(oggstream*)g_hash_table_lookup(hash,GINT_TO_POINTER(serial));
		}
		/* copy the page into the stream */
		ret = ogg_stream_pagein(&stream->mState,&page);
		if (ret!=0) {
			fprintf(stderr,"Error in ogg_stream_page_in() for stream %d\n",serial);
			fclose(fp);
			return EXIT_FAILURE;
		}
		/* see if we have a full packet stored in the stream */
		ogg_packet packet;
		memset(&packet,0,sizeof(packet));
		while (ogg_stream_packetout(&stream->mState,&packet)!=0) {
			int break_early=0;
			/* otherwise, we have a valid packet */
			stream->mPacketCount++;
			/* See if it is a theora packet */
			if (!stream->headers_read) {
				ret=th_decode_headerin(&stream->mTheora.mInfo,
						&stream->mTheora.mComment,
						&stream->mTheora.mSetup,
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
				if (break_early) continue;
				/* otherwise, it is a theora packet */
				if (ret>0) {
					/* then specifically, it is a header packet */
					stream->stream_type= TYPE_THEORA;
					fprintf(stderr,"Stream %d packet %d was a Theora header\n",
							serial,stream->mPacketCount);
					continue;
				}
				/* then we have a video packet */
				if (stream->mTheora.mCtx==NULL)  {
					stream->mTheora.mCtx = th_decode_alloc(
							&stream->mTheora.mInfo,
							stream->mTheora.mSetup);
				}
				if (stream->mTheora.mCtx==NULL) {
					fprintf(stderr,"Failed to allocated theora context for stream %d\n",
							serial);
					return EXIT_FAILURE;
				}
			} else {
				stream->headers_read=1;
			}
			/* decode the data packet */
			fprintf(stderr,"Stream %d packet %d was Theora data\n",
					serial,stream->mPacketCount);
			/* add the packet to the decoder */
			ret=th_decode_packetin(stream->mTheora.mCtx,&packet,&granulepos);
			if (ret==0) {
				ret = th_decode_ycbcr_out(stream->mTheora.mCtx,buffer);
				assert(ret==0);
				/* ok, buffer should now be YUV data */
				save_ppm_image(&stream->mTheora.mInfo,buffer,stream->mPacketCount);
			}
		}
	}
	fclose(fp);

	print_header_info(&stream->mTheora.mInfo);

	GList *vals = g_hash_table_get_values(hash);
	GList *tlist=NULL;
	for (tlist=vals;tlist!=NULL;tlist=g_list_next(tlist)) {
		stream=(oggstream*)(tlist->data);
		fprintf(stderr,"Stream %d contained %d packets\n",
				stream->mSerial,
				stream->mPacketCount);
	}
	g_list_free(tlist);
	g_hash_table_destroy(hash);

	return EXIT_SUCCESS;
}
