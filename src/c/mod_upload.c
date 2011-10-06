// I don't really know why this is needed 
// but it seems to make APR includes work on my 32bit linux
// off64_t is for large files over 2Gb
// I don't know if it is safe to hardcode this but
// fuckif I know how Apache autoconf works!
// tried signed long long and just unsigned long to no avail (sigsev)
#ifndef off64_t
# define off64_t unsigned long long
#endif

/**
 * @author teknopaul
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */
#include "httpd.h"
#include <apr_strings.h>
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include <string.h>
#include "multipart_parser.h"
#include "mpp_flusher.h"

module AP_MODULE_DECLARE_DATA upload_module;

typedef struct {
  char* ok_file;
  char* fail_file;
  int allow_overwrite; // 0 == true
} upload_conf ;

static void* upload_create_config(apr_pool_t* p, char* x) {
  upload_conf* conf = apr_pcalloc(p, sizeof(upload_conf)) ;
  conf->ok_file = "/var/www/html/ok.html" ;
  conf->fail_file = "/var/www/html/fail.html" ;
  conf->allow_overwrite = 1;
  return conf ;
}
static const char* set_ok_file(cmd_parms* cmd, void* cfg, const char* name) {
  upload_conf* conf = (upload_conf*) cfg ;
  conf->ok_file = apr_pstrdup(cmd->pool, name) ;
  return NULL ;
}
static const char* set_fail_file(cmd_parms* cmd, void* cfg, const char* name) {
  upload_conf* conf = (upload_conf*) cfg ;
  conf->fail_file = apr_pstrdup(cmd->pool, name) ;
  return NULL ;
}
static const char* set_allow_overwrite(cmd_parms* cmd, void* cfg, const char* name) {
  upload_conf* conf = (upload_conf*) cfg ;
  conf->allow_overwrite = strcmp(name, "true");
  return NULL ;
}
/*
 * return pointer to a file or 0 for all errors
 * 0 is returned as error since parse_ctx is memset to 0 for all data at the start
 * rather too much code, but security is an issue
 */
static apr_file_t *create_file(request_rec *r, parse_ctx *ctx, upload_conf *conf) {
  // validate the file name
  char c = ctx->file_name[0];
  switch (c) {
    case '\0' : return 0; // no file
    case '.'  : return 0; // hidden file
    case '/'  : return 0; 
    case '\\' : return 0;
  }
  int len = strlen(ctx->file_name);
  if (len > 100) { // TODO long DocumentRoot root paths might overflow
    return 0;
  }
  int i = 0;
  for (; i < len ; i++) {
    c = ctx->file_name[i];
    switch (c) {
      case '/'  : return 0; // TODO might be part of a valid utf-8 code
      case '\\' : return 0; // TODO might be part of a valid utf-8 code
    }
  }

// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "filename [%s]", r->filename);
  char save_file[256];
  save_file[0] = 0;
  char* slashIndex = strrchr(r->filename, '/');
  if (slashIndex == NULL) {
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "missing slash [%s]", r->filename);
    return 0;
  }
  else {
    slashIndex++; // can not be last char, file must end in .upload to get here, TODO not for SetHandler use
    *slashIndex = 0;// terminate the string at the end of the directory path
  }
  strcat(save_file, r->filename); // the path not the file name
  strcat(save_file, ctx->file_name);
  apr_file_t* fle = NULL;
  apr_fileperms_t perm = APR_UREAD; // create a readable file
  if (conf->allow_overwrite == 0 ) {
    perm = perm | APR_UWRITE; // create a read and writable file if overwriting is permitted
  }
  apr_int32_t flags = APR_CREATE | APR_WRITE | APR_BINARY;
  if (conf->allow_overwrite != 0 ) {
    flags = flags | APR_EXCL;
  }
  apr_status_t fstatus;
  fstatus = apr_file_open(&fle, save_file, flags, perm, r->pool);
  if (fle == NULL) {
    char errbuf[256];
    apr_strerror(fstatus, errbuf, 255); // get the error as a string
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "apr_file_open [%s] [%s]", errbuf, save_file);
    return 0;
  }
  if (fstatus < 0) { // APR docs do not define return types Unix open.c returns < 0 as errno AFAIK 0 == SUCCESS
    char errbuf[256];
    apr_strerror(fstatus, errbuf, 255);
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "apr_file_open failed [%s] [%s] [%s]", save_file, r->path_info, errbuf);
    return 0;
  }
// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "apr_file_open [%s] [%s] [%d] [%d]", r->filename, r->path_info, fstatus, fle);
  return fle;
}

static const char* get_boundary(apr_pool_t* p, const char* ctype) {
// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, p, "Content-Type [%s]", ctype);
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

static int emit_file(request_rec *r, const char *name) {
  int res;
  apr_file_t *f;
  apr_status_t fstatus;
  apr_finfo_t fi;

  apr_fileperms_t perm = APR_OS_DEFAULT;
  fstatus = apr_file_open(&f, name, APR_READ, perm, r->pool);
  if (fstatus != APR_SUCCESS) {
    return fstatus;
  }

  if (f == NULL) {
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "mod_upload incorrectly configured, can not access %s", name);
    return HTTP_FORBIDDEN;
  }

  if ((res = apr_stat(&fi, name, APR_FINFO_SIZE, r->pool)) != APR_SUCCESS) {
    return HTTP_FORBIDDEN;
  }

  ap_set_content_length(r, fi.size);

  ap_set_content_type(r, "text/html");
  //register_timeout ("send", r);
  //ap_send_http_header(r);

  if ( ! r->header_only) {
    apr_size_t sent;
    ap_send_fd(f, r, 0, fi.size, &sent);
  }
  apr_file_close(f);
  return OK;
}
static void emit_ok(request_rec *r, upload_conf *conf) {
  emit_file(r, conf->ok_file);
}

static void emit_fail(request_rec *r, upload_conf *conf) {
  emit_file(r, conf->fail_file);
}

/* here's the content handler */
static int upload_handler(request_rec *r) {
  // modules seem to ALWAYS run so disable this module if not explicitly set

// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "init upload-handler");

  if( strcmp(r->handler, "upload-handler") ) {
        // ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "Not the handler we are looking for");
        return DECLINED;
  }

// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "running upload-handler");
// from http://modules.apache.org/doc/API.html#HMR                       
//  r->uri                                                               
//  r->filename                                                          
//  r->path_info                                                         
//  r->method; // GET or POST                                            
//  r->no_cache // int                                                   
//  r->per_dir_config //Options set in config files, etc.                
//  r->request_config // Notes on *this* request                                                                                                                                
//  if (r->finfo.st_mode == 0) return NOT_FOUND;                                                                                                                                

  if (strcmp( r->method, "POST" ) != 0 ) {
    return DECLINED;
  }

  const char* ctype = apr_table_get(r->headers_in, "Content-Type") ;
  if ( ! ctype || strncmp ( ctype , "multipart/form-data", 19 ) ) {
    return HTTP_BAD_REQUEST ;
  }

// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "reading request");                                 
  ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK);                                                      

  upload_conf *conf = (upload_conf *)ap_get_module_config(r->per_dir_config,
                                                      &upload_module);
// stack based buffers
  parse_ctx sctx;
  parse_ctx *ctx = &sctx;
  memset( ctx, 0, sizeof(parse_ctx) );
  const char* boundary = get_boundary(r->pool, ctype);
  if (strlen(boundary) > 70) {
    return HTTP_BAD_REQUEST;
  }
  // boundary has "--" prefixed
  char boundaryPrefix[72];
  boundaryPrefix[0] = '-';
  boundaryPrefix[1] = '-';
  boundaryPrefix[2] = 0;
  strcat(boundaryPrefix, boundary);
  
  ctx->boundary = boundaryPrefix;
  ctx->boundary_len = strlen(boundaryPrefix);

// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "boundary=[%d][%s]", ctx->boundary_len, ctx->boundary);
  char buffer[1024];
  long read = 0;

  do {
    read = ap_get_client_block(r, buffer, (apr_size_t)1024);
// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "read [%d] [%s]", read, buffer);
    int i = 0;
    int ret = 0;
    for (i = 0 ; i < read ; i++) {
      ret = mpp_process_char(ctx, buffer[i]);
// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "[%c][%d][%d][%d]", buffer[i], ctx->reading_data, ctx->reading_meta, ctx->reading_unknown);
      if (ret > 1) {
//        ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "flushed");
      }
// TODO handle disk full errors 
      if (ret < 0 ) {
        ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "failed processing upload [%d]", ret);
      	return HTTP_BAD_REQUEST;
      }
      if (ctx->user_data == 0 && ctx->file_name[0] != 0) {
        ctx->user_data = create_file(r, ctx, conf);
        if (ctx->user_data == 0) { // fileserver permissions errors and other stuff
          emit_fail(r, conf);
          return OK;
        }
      }
    }
  } while(read != 0);
  if (ctx->user_data != 0) {
//    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "Saved file [%s][%d][%d][%d]", ctx->file_name, ctx->written, ctx->user_data, ctx->data_pos);
    apr_file_close(ctx->user_data);
  }

// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "read request");

  emit_ok(r, conf);
  return OK;
}

static void *merge_upload_configs(apr_pool_t *p, void *basev, void *addv)
{
    upload_conf *new;
    upload_conf *base = (upload_conf *) basev;
    upload_conf *add = (upload_conf *) addv;
    new = (upload_conf *) apr_pcalloc(p, sizeof(upload_conf));
    new->ok_file = add->ok_file ? add->ok_file
                                          : base->ok_file;
    new->fail_file = add->fail_file ? add->fail_file
                                          : base->fail_file;
    new->allow_overwrite = add->allow_overwrite ? add->allow_overwrite
                                          : base->allow_overwrite;
    return new;
}

static const command_rec upload_cmds[] = {
  AP_INIT_TAKE1("UploadOkHtml", set_ok_file, NULL, OR_ALL,
    "Set thehtml file to display when upload has finished" ) ,
  AP_INIT_TAKE1("UploadFailHtml", set_fail_file, NULL, OR_ALL,
    "Set thehtml file to display when upload has finished" ) ,
  AP_INIT_TAKE1("UploadAllowOverwrite", set_allow_overwrite, NULL, OR_ALL,
    "when true overriting files is permitted" ) ,
  {NULL}
} ;

/* Make the name of the content handler known to Apache */
static void register_hooks (apr_pool_t *p)
{
        // I think this is the call to make to register a handler for method calls (GET PUT et. al.).
        ap_hook_handler(upload_handler, NULL, NULL, APR_HOOK_LAST);
}

module AP_MODULE_DECLARE_DATA upload_module =
{
        // Only one callback function is provided.  Real
        // modules will need to declare callback functions for
        // server/directory configuration, configuration merging
        // and other tasks.
        STANDARD20_MODULE_STUFF,
        upload_create_config,
        merge_upload_configs,
        NULL,
        NULL,
        upload_cmds,
        register_hooks, /* callback for registering hooks */
};
