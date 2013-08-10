#include <stdio.h>
#include <string.h>
#include "multipart_parser.h"
#include "mpp_flusher.h"

#define DEBUG false

/**
 * Push the data through the filters
 */
static int flush_data_buffer(parse_ctx *ctx) {
  int ret = mpp_flush_data(ctx->data_buffer, ctx->data_pos, ctx);
  if (ret >= 0) {
    ctx->data_pos = 0; 
    memset(ctx->data_buffer, 0, DATA_BUFFER_SIZE);
  }
  return ret;
}
/**
 * Push the "unknown" through the filters (when confirmed to be part of data and not boundary)
 */
static int flush_unknown_buffer(parse_ctx *ctx) {
  int ret = mpp_flush_data(ctx->unknown_buffer, ctx->unknown_pos, ctx);
  if ( ret >= 0 ) {
    ctx->unknown_pos = 0; 
    memset(ctx->unknown_buffer, 0, UNKNOWN_BUFFER_SIZE); 
  }
  return ret;
}

static void discard_meta(parse_ctx *ctx){
  ctx->meta_pos = 0;
  //ctx->meta_buffer[0] = 0;
  memset(ctx->meta_buffer, 0, META_BUFFER_SIZE);
}

static void extract_filename(parse_ctx *ctx) {

  char *start = ctx->meta_buffer;
  for ( ; *start == ' ' ; start++ );

  char* colon = strchr(ctx->meta_buffer, ':' ) ;
  if ( colon ) {
    *colon++ = 0 ;
    while ( *colon == ' ' ) {
      ++colon ;
    }
    if ( strncasecmp( start , "Content-Disposition", 19 ) == 0 ) {
      char* name_idx = strstr(colon, "name=") ;
      if ( name_idx ) {
        name_idx = strstr(colon, "name=\"") ;
      }
      if ( name_idx ) {
        char* end_idx;
        char* nxt;
        name_idx += 6 ;
        end_idx = strchr(name_idx, '"') ;
        if ( end_idx ) {
          char *field_name = name_idx; // TODO take copy "apr_pstrndup()" and pass to handler
          nxt = end_idx;
          *end_idx = 0 ;
 fprintf(stdout,  "field_name [%s]\n", field_name);
          if ( ! strcmp(field_name, "files[]") ) { // TODO configurable
            ctx->reading_file = 1;
            // get the name of the uploaded file
            nxt++;;
            char* filename_idx = strstr(nxt, "filename=\"") ;
            if ( filename_idx ) {
              filename_idx += 10 ;
              end_idx = strchr(filename_idx, '"') ;
              if ( end_idx ) {
                memcpy ( ctx->file_name, filename_idx, end_idx - filename_idx);
 fprintf(stdout,  "file_name [%s]\n", ctx->file_name);
              }
            }

          }
        }

      }
    }
  }
}


/**
 * return 0 if found filename
 */
static int process_meta_newline(parse_ctx *ctx) {

  char *start = ctx->meta_buffer;
  for ( ; *start == ' ' ; start++ );

  if ( strchr(start, ':') != NULL ) { // we have a header
// fprintf(stdout,  "Header found [%s]\n", ctx->meta_buffer);
    if ( strncasecmp( start , "Content-Type" , 12) == 0 ) {
// fprintf(stdout,  "discard Content-Type\n");
      discard_meta(ctx);
      return 1;
    }
    else if ( strncasecmp( start , "Content-Disposition", 19 ) == 0 ) {
// fprintf(stdout,  "found Content-Disposition\n");
      extract_filename(ctx);
      discard_meta(ctx);
      return 0;
    }
  }
  else {
// fprintf(stdout,  "discard meta [%s]\n", ctx->meta_buffer);
    discard_meta(ctx);
  }
  return 1;
}


/*
static void empty_meta_buffer(parse_ctx *ctx) {
  ctx->meta_pos = 0; 
  memset(ctx->meta_buffer, 0, META_BUFFER_SIZE); 
}
*/

/**
 * return -1 no match, 0 matching, 1 full match
 */
static int match_boundary(parse_ctx *ctx, char c) {
// fprintf(stdout,  "%c", c);
  if (ctx->boundary[ctx->unknown_pos] == c) {
    if (ctx->unknown_pos == ctx->boundary_len - 1) {
      return 1;
    }
    return 0;
  }
  return -1;
}



int mpp_process_char(parse_ctx *ctx, char c) {
  // trap errors first
  if (ctx->meta_pos >= META_BUFFER_SIZE - 1 ||
      ctx->data_pos >= DATA_BUFFER_SIZE - 1 ||
      ctx->unknown_pos >= UNKNOWN_BUFFER_SIZE - 1) {
// fprintf(stdout, "Buffer limit reached meta[%d] data[%d] unknown[%d]\n", ctx->meta_pos, ctx->data_pos, ctx->unknown_pos);
    return 1;
  }
 // discard leading new lines
  if ( ctx->reading_meta == 0 && ctx->reading_data == 0 && ctx->reading_unknown == 0 ) {
    if ( c == '\n' || c == '\r' ) {
      return 0;
    }
    else {
// fprintf(stdout, "Started parse of data\n");
      ctx->reading_meta = 1;
      ctx->meta_buffer[ctx->meta_pos] = c;
      ctx->meta_pos++;
      return 0;
    }
  }

  // hit a new line char
  if ( c == '\n' ) {
// fprintf(stdout, "newline\n");
    if ( ctx->reading_meta == 1 ) {
      if ( ctx->last_was_newline == 1 ) {
// fprintf(stdout, "\n double newline end of meta \n\n");
        ctx->last_was_newline = 0;
        ctx->reading_meta = 0;
        ctx->reading_data = 1;
        discard_meta(ctx);
      }
      else {
        ctx->meta_buffer[ctx->meta_pos] = 0;
        process_meta_newline(ctx);
        ctx->last_was_newline = 1;
      }
      return 0;
    }
    else if ( ctx->reading_unknown == 1 ) {
// fprintf(stdout, "newline in unknown_state always wrong [%d]\n", c);
      ctx->unknown_buffer[ctx->unknown_pos] = c;
      ctx->unknown_pos++;
      flush_data_buffer(ctx);
      flush_unknown_buffer(ctx);
      ctx->reading_unknown = 0;
      ctx->reading_data = 1;
      return 0;
    }
    else if ( ctx->reading_data == 1 ) {
// fprintf(stdout, "enter unknown state\n");
      ctx->data_buffer[ctx->data_pos] = c;
      ctx->data_pos++;
      ctx->reading_unknown = 1 ;
      return 0;
    }
    return -1; //bug
  }

  if ( c == '\r' ) {
    if ( ctx->reading_meta == 1 ) {
      return 0;
    }
  }

  // any other char c
  ctx->last_was_newline = 0;
  if ( ctx->reading_unknown == 1 ) {
// fprintf(stdout, "reading_unkown");
    int ret = match_boundary(ctx, c);
    if ( ret == 0 ) { // matching
      ctx->unknown_buffer[ctx->unknown_pos] = c;
      ctx->unknown_pos++;
      return 0;
    }
    else if ( ret == 1 ) { // full match
// fprintf(stdout, "Match!\n");

      // discard CRLF we added, this works for only LF provided there was not a natural CR first (spec requries CRLF anyway)
      if (ctx->data_buffer[ctx->data_pos -1] == '\n') ctx->data_pos--;
      if (ctx->data_buffer[ctx->data_pos -1] == '\r') ctx->data_pos--;
      ctx->data_buffer[ctx->data_pos] = 0; // string terminate (not needed except for debug)
      int ret = flush_data_buffer(ctx);
      ctx->unknown_buffer[0] = 0;
      ctx->unknown_pos = 0;
      ctx->reading_file = 0;
      ctx->reading_unknown = 0;
      ctx->reading_data = 0;
      ctx->reading_meta = 1;
      return ret;
    }
    else {
// fprintf(stdout, "False alarm!!\n");
      ctx->unknown_buffer[ctx->unknown_pos] = c;
      ctx->unknown_pos++;
      int ret = flush_data_buffer(ctx);
      flush_unknown_buffer(ctx);
      ctx->reading_unknown = 0;
      return ret;
    }
  }
  // fill the buffers
  else if (ctx->reading_meta == 1) {
// fprintf(stdout, "filling meta [%d]\n" , ctx->meta_pos);
    ctx->meta_buffer[ctx->meta_pos] = c;
    ctx->meta_pos++;
    return 0;
  }    
  else if (ctx->reading_data == 1) {
    int ret = 0;
    if (ctx->data_pos == DATA_BUFFER_SIZE - 2) { // -2 for CRLF that can end up in the data_buffer
      // flush the data_buffer, it is full
      ret = flush_data_buffer(ctx);
    }
    ctx->data_buffer[ctx->data_pos] = c;
    ctx->data_pos++;
    return ret;
  }
  else {
// fprintf(stdout, "bug\n");
    return -2;
  }
}

/**
 * Can be called after last byte is sent to finish up, on the off chance we did not
 * end with a valid trailer
 */
int mpp_finish(parse_ctx *ctx) {
    ctx->data_buffer[ctx->data_pos] = 0;
// fprintf(stdout, "finish leftover[%s]\n", ctx->data_buffer);
  if (ctx->reading_meta == 0 && ctx->reading_unknown == 0) {
    flush_data_buffer(ctx);
    flush_unknown_buffer(ctx);
    return -1;
  }
  else if (ctx->reading_meta == 0 && ctx->reading_data == 1) {
    flush_data_buffer(ctx);
    return -1;
  }
  else {
    return 0;
  }
}
