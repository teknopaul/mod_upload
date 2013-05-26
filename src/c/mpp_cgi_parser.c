#include "multipart_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
/*
 * return pointer to a file or 0 for all errors
 * 0 is returned as error since parse_ctx is memset to 0 for all data at the start
 */
static int create_file(const char* dir, const char* overwrite, parse_ctx *ctx) {
	// validate the file name
	char c = ctx->file_name[0];
	switch (c) {
		case '\0' : return 0;
		case '.'  : return 0;
		case '/'  : return 0;
		case '\\' : return 0;
	}
	int len = strlen(ctx->file_name);
	if (len > 200) {
		return -1;
	}
	int i = 0;
	for (; i < len ; i++) {
		c = ctx->file_name[i];
		switch (c) {
			case '/'  : return 0;
			case '\\' : return 0;
		}
	}


	char save_file[256];
	strcat(save_file, dir); // the path not the file name
	strcat(save_file + strlen(dir), ctx->file_name);

	// open file to write
	int flags = O_WRONLY;
	if ( *overwrite == 't' ) {
		flags |= O_CREAT;
	}
	else {
		flags |= O_CREAT | O_EXCL;
	}
	
	int outfd = open(save_file, flags);
	
	if (outfd == -1) {
		fprintf(stderr, "Unable to open %s\n", save_file);
		return -1;
	}
	
//	fprintf(stderr, "Opened %s for writing\n", save_file);

	int *fd = malloc(sizeof(int));
	if (fd == NULL) return -1;
	*fd = outfd;
	
	ctx->user_data = fd;
	
	return 0;
}

/**
 * Extract boundry from this string	
 * Content-Type: multipart/form-data; boundary=---------------------------52076228513486065321795096073
 */
static const char* get_boundary(const char* ctype) {
	if (strlen(ctype) < 10) {
		return NULL;
	}
	
	const char* ret = NULL ;
	if ( ctype ) {
		const char *start = ctype;
		for ( ; *start == ' ' ; start++ );
		const char* semi = strchr(start, ';' ) ;
		semi++;
		for ( ; *semi == ' ' ; semi++ );
		if ( strncasecmp( semi , "boundary", 8 ) == 0 ) {
			const char* boundary = semi+8;
			for ( ; *boundary == ' ' ; boundary++ );
			if (*boundary == '=') {
				boundary++;
				for ( ; *boundary == ' ' ; boundary++ );
				return boundary;
			}
		}
	}
	return ret ;
}

/**
 * Parses a multipart/form-data HTTP body stream, the HTTP headers and query string, should have been parsed already.
 * 
 * Call supplying 3 arguments
 *  - The directory to save files.
 *  - The Content-Type header from GCI which includes the boundry used to delimit the binary data.
 *  - Argument indicating if existing files should be overriden
 */
int main(int argc, const char *argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Requires Directory to save files, Content-Type header and overwrite flag as the only arguments\n");
		return 2;
	}
	
	parse_ctx sctx;
	parse_ctx *ctx = &sctx;
	memset( ctx, 0, sizeof(parse_ctx) );	
	
	ctx->boundary = get_boundary(argv[2]);
	if (ctx->boundary == NULL) {
		fprintf(stderr, "Unable to parse boundry from Content-Type \"%s\"\n", argv[2]);
		return 2;
	}
	
	ctx->boundary_len = strlen(ctx->boundary);
	int i;
	int ret;
	int c;

	while( (c = getc(stdin) ) != EOF) {
		ret = mpp_process_char(ctx, c);
		if (ret != 0 ) {
			fprintf(stderr,"Failed [%d]\n", ret );
			return 2;
		}
		if (ret > 1) {
//				ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "flushed");
		}
		// TODO handle disk full errors 
		if (ret < 0 ) {
			fprintf(stderr,"Failed processing upload [%d]\n", ret);
			return 2;
		}
		if (ctx->user_data == 0 && ctx->file_name[0] != 0) {
			create_file(argv[1], argv[3], ctx);
			if (ctx->user_data == 0) {
				fprintf(stderr, "Cant create file permissions errors?\n");
				return 3;
			}
		}
	}
	
	mpp_finish(ctx) ;
	
	fprintf (stderr, "File [%s] created\n", ctx->file_name);
	return ret;
};
