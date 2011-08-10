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
		ret = ogg_stream_packetout(&stream->mState,&packet);
		if (ret==0) {
			/* then a packet is not yet ready */
			continue;
		} else if (ret==-1) {
			fprintf(stderr,"There is a break in the data! Page lost?\n");
			continue;
		}
		/* otherwise, we have a valid packet */
		stream->mPacketCount++;
		/* See if it is a theora packet */
		ret=th_decode_headerin(&stream->mTheora.mInfo,
				&stream->mTheora.mComment,
				&stream->mTheora.mSetup,
				&packet);
		if (ret==TH_ENOTFORMAT) {
			fprintf(stderr,"Not TH_ENOTFORMAT, but %d\n",ret);
			continue;
		}
		/* otherwise, it is a theora packet */
		if (ret>0) {
			/* then specifically, it is a header packet */
			stream->stream_type= TYPE_THEORA;
			fprintf(stderr,"Stream %d packet %d was a Theora header\n",
					serial,stream->mPacketCount);
			continue;
		}
		/* then we have a video packet */
		if (!stream->headers_read) {
			stream->mTheora.mCtx = th_decode_alloc(
					&stream->mTheora.mInfo,
					stream->mTheora.mSetup);
			if (stream->mTheora.mCtx==NULL) {
				fprintf(stderr,"Failed to allocated theora context for stream %d\n",
						serial);
				return EXIT_FAILURE;
			}
		} else {
			stream->headers_read=1;
		}
		/* decode the data packet */
		/* add the packet to the decoder */
		ogg_int64_t granulepos=-1;
		ret=th_decode_packetin(stream->mTheora.mCtx,&packet,&granulepos);
		assert(ret==0);
		th_ycbcr_buffer buffer;
		ret = th_decode_ycbcr_out(stream->mTheora.mCtx,buffer);
		assert(ret==0);
		/* ok, buffer should now be YUV data */
		/* do something with it...*/
		fprintf(stderr,"Stream %d packet %d was Theora data\n",
				serial,stream->mPacketCount);
	}
	fclose(fp);

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
