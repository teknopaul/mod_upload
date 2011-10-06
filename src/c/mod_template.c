/*
 *  Template for createing Apache2 content handler modules
 *  replace the text "template" with your module name
 *
 *  compile with
 *  gcc -Wall -shared -fPIC -I/usr/include/httpd -I/usr/include/apr-1 -c mod_template.c
 *  TODO deal with APR's hideous build system, this works on 64bit Fedora but not 32bit
 *
 * @author Paul Hinds
 */
#include "httpd.h"
#include "apr_strings.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"

module AP_MODULE_DECLARE_DATA template_module ;

// LOCAL DEFS (or in header)
typedef struct {
  char* my_option1;
  int my_option2;
} template_conf ;

// LOCAL METHODS (pass the request_rec object around)
static int do_thing(request_rec *r) {
  ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "error logging [%s]", "wrong");
  return 0;
}

// CONFIG HANDLERS
static void* template_create_config(apr_pool_t* p, char* x) {
  template_conf* conf = apr_pcalloc(p, sizeof(template_conf)) ;
  conf->my_option2 = 23 ;
  return conf ;
}
static const char* set_my_option(cmd_parms* cmd, void* cfg, const char* name) {
  template_conf* conf = (template_conf*) cfg ;
  conf->my_option1 = apr_pstrdup(cmd->pool, name) ;
  return NULL ;
}

// RESPONSES
static void emit_ok(request_rec *r) {
  ap_set_content_type(r, "text/html");
  ap_rputs("<html><head><link href=\"/template/style.css\" rel=\"stylesheet\" type=\"text/css\">",r);
  ap_rputs("<title><title/></head>",r);
  ap_rputs("<body>ok</body></html>",r);
}
static void emit_fail(request_rec *r) {
  ap_rputs("<html><head><link href=\"/template/style.css\" rel=\"stylesheet\" type=\"text/css\">",r);
  ap_rputs("<title><title/></head>",r);
  ap_rputs("<body>failed</body></html>",r);
}


// HANDLER
static int template_handler(request_rec *r) {
  // handlers are always called so bomb out early if we are not handling this request
  if( strcmp(r->handler, "template-handler") ) {
    return DECLINED;
  }
  // be specific about the requires type to handle
  if (strcmp( r->method, "GET" ) != 0 ) {
    return DECLINED;
  }

  // ... handler code
  if ( do_thing(r) < 0) {
    emit_fail(r);
  }
  else {
    emit_ok(r);
  }

  return OK;
}

// STANDARD APACHE MODULE STUFF
static const command_rec template_cmds[] = {
  AP_INIT_TAKE1("MyOption", set_my_option, NULL, OR_ALL,
    "Set value of MyOption" ) ,
  {NULL}
} ;

/* Make the name of the content handler known to Apache */
static void template_hooks(apr_pool_t *p)
{
  // This is the call to make to register a handler for method calls (GET PUT et. al.).
  ap_hook_handler(template_handler, NULL, NULL, APR_HOOK_LAST);
}


module AP_MODULE_DECLARE_DATA template_module = {
  STANDARD20_MODULE_STUFF,
  template_create_config,
  NULL,
  NULL,
  NULL,
  template_cmds,
  template_hooks
} ;

