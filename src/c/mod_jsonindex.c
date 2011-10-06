// I don't really know why this is needed, but it seems to make APR includes work on my 32bit linux
#ifndef off64_t
# define off64_t unsigned long long
#endif
/*
 *  Directory idexing returning JSON
 *
 *  compile with
 *  gcc -Wall -shared -fPIC -I/usr/include/httpd -I/usr/include/apr-1 -c mod_jsonindex.c
 *  TODO deal with APR's hideous build system, this works on 64bit Fedora but not 32bit
 *
 * @author Paul Hinds, Rob McCool, Martin Pool (see mod_autoindex for much of the original code)
 */
#include "apr_strings.h"
#include "apr_fnmatch.h"
#include "apr_strings.h"
#include "apr_lib.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_log.h"
#include "http_main.h"
#include "util_script.h"

#include "mod_core.h"


module AP_MODULE_DECLARE_DATA jsonindex_module ;

// LOCAL DEFS (or in header)

#define NO_OPTIONS          (1 <<  0)  /* Indexing options */
#define SUPPRESS_ICON       (1 <<  3)
#define SUPPRESS_LAST_MOD   (1 <<  4)
#define SUPPRESS_SIZE       (1 <<  5)
#define SUPPRESS_DESC       (1 <<  6)
#define SUPPRESS_PREAMBLE   (1 <<  7)
#define SUPPRESS_COLSORT    (1 <<  8)
#define SUPPRESS_RULES      (1 <<  9)
#define FOLDERS_FIRST       (1 << 10)
#define VERSION_SORT        (1 << 11)
#define TRACK_MODIFIED      (1 << 12)
#define IGNORE_CLIENT       (1 << 15)
#define IGNORE_CASE         (1 << 16)
#define SHOW_FORBIDDEN      (1 << 18)
#define DO_JSON_INDEXES     (1 << 23)

#define K_NOADJUST 0
#define K_ADJUST 1
#define K_UNSET 2

/*
 * Define keys for sorting.
 */
#define K_NAME 'N'              /* Sort by file name (default) */
#define K_LAST_MOD 'M'          /* Last modification date */
#define K_SIZE 'S'              /* Size (absolute, not as displayed) */
#define K_DESC 'D'              /* Description */
#define K_VALID "NMSD"          /* String containing _all_ valid K_ opts */

#define D_ASCENDING 'A'
#define D_DESCENDING 'D'
#define D_VALID "AD"            /* String containing _all_ valid D_ opts */

struct item {
    char *type;
    char *apply_to;
    char *apply_path;
    char *data;
};

typedef struct ai_desc_t {
    char *pattern;
    char *description;
    int full_path;
    int wildcards;
} ai_desc_t;

typedef struct jsonindex_conf {
    char *json_index_file;
    char *default_icon;
    apr_int32_t opts;
    apr_int32_t incremented_opts;
    apr_int32_t decremented_opts;
    char default_keyid;
    char default_direction;

    apr_array_header_t *icon_list;
    apr_array_header_t *alt_list;
    apr_array_header_t *desc_list;
    apr_array_header_t *ign_list;
    apr_array_header_t *hdr_list;
    apr_array_header_t *rdme_list;

} jsonindex_conf;

static char c_by_encoding, c_by_type, c_by_path;

#define BY_ENCODING &c_by_encoding
#define BY_TYPE &c_by_type
#define BY_PATH &c_by_path

/* Structure used to hold entries when we're actually building an index */
struct ent {
    char *name;
    char *icon;
    char *alt;
    char *desc;
    apr_off_t size;
    apr_time_t lm;
    struct ent *next;
    int ascending, ignore_case, version_sort;
    char key;
    int isdir;
};
// LOCAL METHODS (pass the request_rec object around)

/*
 * escape text to a JSON string, up to n characters are processed
 * assumes input is encoded properly
 */
static char* escapen_json(apr_pool_t* p, const char* input, int n) {
  const char* ipos = input;
  int len = strlen(input) * 2;
  len++;
  char *output = apr_pcalloc(p, sizeof(char) * len);
  memset( output, 0, sizeof(char) * len );
  int opos = 0;
  int i;
  for (i = 0; i < n && i < len; i++, ipos++) {
    char c = *ipos;
    if (c == 0 ) {
        output[opos] = 0;
        break;
    }
    switch(c) {
      case '\"':
        output[opos] = '\\';
        break;
      case '\'':
        output[opos] = '\'';
        break;
      case '\\':
        output[opos] = '\\';
        break;
      case '\n':
        output[opos] = '\\';opos++;
        output[opos] = 'n';
        continue;
      case '\r':
        output[opos] = '\\';opos++;
        output[opos] = 'r';
        continue;
      case '\t':
        output[opos] = '\\';opos++;
        output[opos] = 't';
        continue;
    }
// ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, p, "escape [%c][%s][%d]", c, output, i);
    output[opos] = c;
    opos++;
  }
  return output; // broken
}



static void push_item(apr_array_header_t *arr, char *type, const char *to,
                      const char *path, const char *data)
{
    struct item *p = (struct item *) apr_array_push(arr);

    if (!to) {
        to = "";
    }
    if (!path) {
        path = "";
    }

    p->type = type;
    p->data = data ? apr_pstrdup(arr->pool, data) : NULL;
    p->apply_path = apr_pstrcat(arr->pool, path, "*", NULL);

    if ((type == BY_PATH) && (!ap_is_matchexp(to))) {
        p->apply_to = apr_pstrcat(arr->pool, "*", to, NULL);
    }
    else if (to) {
        p->apply_to = apr_pstrdup(arr->pool, to);
    }
    else {
        p->apply_to = NULL;
    }
}

static const char *add_alt(cmd_parms *cmd, void *d, const char *alt,
                           const char *to)
{
    if (cmd->info == BY_PATH) {
        if (!strcmp(to, "**DIRECTORY**")) {
            to = "^^DIRECTORY^^";
        }
    }
    if (cmd->info == BY_ENCODING) {
        char *tmp = apr_pstrdup(cmd->pool, to);
        ap_str_tolower(tmp);
        to = tmp;
    }

    push_item(((jsonindex_conf *) d)->alt_list, cmd->info, to,
              cmd->path, alt);
    return NULL;
}

static const char *add_icon(cmd_parms *cmd, void *d, const char *icon,
                            const char *to)
{
    char *iconbak = apr_pstrdup(cmd->pool, icon);

    if (icon[0] == '(') {
        char *alt;
        char *cl = strchr(iconbak, ')');

        if (cl == NULL) {
            return "missing closing paren";
        }
        alt = ap_getword_nc(cmd->pool, &iconbak, ',');
        *cl = '\0';                             /* Lose closing paren */
        add_alt(cmd, d, &alt[1], to);
    }
    if (cmd->info == BY_PATH) {
        if (!strcmp(to, "**DIRECTORY**")) {
            to = "^^DIRECTORY^^";
        }
    }
    if (cmd->info == BY_ENCODING) {
        char *tmp = apr_pstrdup(cmd->pool, to);
        ap_str_tolower(tmp);
        to = tmp;
    }

    push_item(((jsonindex_conf *) d)->icon_list, cmd->info, to,
              cmd->path, iconbak);
    return NULL;
}

/*
 * Add description text for a filename pattern.  If the pattern has
 * wildcards already (or we need to add them), add leading and
 * trailing wildcards to it to ensure substring processing.  If the
 * pattern contains a '/' anywhere, force wildcard matching mode,
 * add a slash to the prefix so that "bar/bletch" won't be matched
 * by "foobar/bletch", and make a note that there's a delimiter;
 * the matching routine simplifies to just the actual filename
 * whenever it can.  This allows definitions in parent directories
 * to be made for files in subordinate ones using relative paths.
 */

/*
 * Absent a strcasestr() function, we have to force wildcards on
 * systems for which "AAA" and "aaa" mean the same file.
 */
#ifdef CASE_BLIND_FILESYSTEM
#define WILDCARDS_REQUIRED 1
#else
#define WILDCARDS_REQUIRED 0
#endif

static const char *add_desc(cmd_parms *cmd, void *d, const char *desc,
                            const char *to)
{
    jsonindex_conf *dcfg = (jsonindex_conf *) d;
    ai_desc_t *desc_entry;
    char *prefix = "";

    desc_entry = (ai_desc_t *) apr_array_push(dcfg->desc_list);
    desc_entry->full_path = (ap_strchr_c(to, '/') == NULL) ? 0 : 1;
    desc_entry->wildcards = (WILDCARDS_REQUIRED
                             || desc_entry->full_path
                             || apr_fnmatch_test(to));
    if (desc_entry->wildcards) {
        prefix = desc_entry->full_path ? "*/" : "*";
        desc_entry->pattern = apr_pstrcat(dcfg->desc_list->pool,
                                          prefix, to, "*", NULL);
    }
    else {
        desc_entry->pattern = apr_pstrdup(dcfg->desc_list->pool, to);
    }
    desc_entry->description = apr_pstrdup(dcfg->desc_list->pool, desc);
    return NULL;
}

static const char *add_ignore(cmd_parms *cmd, void *d, const char *ext)
{
    push_item(((jsonindex_conf *) d)->ign_list, 0, ext, cmd->path, NULL);
    return NULL;
}

static const char *add_header(cmd_parms *cmd, void *d, const char *name)
{
    push_item(((jsonindex_conf *) d)->hdr_list, 0, NULL, cmd->path,
              name);
    return NULL;
}

static const char *add_readme(cmd_parms *cmd, void *d, const char *name)
{
    push_item(((jsonindex_conf *) d)->rdme_list, 0, NULL, cmd->path,
              name);
    return NULL;
}

static const char *add_json_index_file(cmd_parms *cmd, void *d, const char *name)
{
    ((jsonindex_conf *) d)->json_index_file = apr_pstrdup(cmd->pool, name) ;
    return NULL;
}

static const char *add_opts(cmd_parms *cmd, void *d, int argc, char *const argv[])
{
    int i;
    char *w;
    apr_int32_t opts;
    apr_int32_t opts_add;
    apr_int32_t opts_remove;
    char action;
    jsonindex_conf *d_cfg = (jsonindex_conf *) d;

    opts = d_cfg->opts;
    opts_add = d_cfg->incremented_opts;
    opts_remove = d_cfg->decremented_opts;

    for (i = 0; i < argc; i++) {
        int option = 0;
        w = argv[i];

        if ((*w == '+') || (*w == '-')) {
            action = *(w++);
        }
        else {
            action = '\0';
        }
        if (!strcasecmp(w, "FoldersFirst")) {
            option = FOLDERS_FIRST;
        }
        else if (!strcasecmp(w, "IgnoreCase")) {
            option = IGNORE_CASE;
        }
        else if (!strcasecmp(w, "IgnoreClient")) {
            option = IGNORE_CLIENT;
        }
        else if (!strcasecmp(w, "SuppressColumnSorting")) {
            option = SUPPRESS_COLSORT;
        }
        else if (!strcasecmp(w, "SuppressDescription")) {
            option = SUPPRESS_DESC;
        }
        else if (!strcasecmp(w, "SuppressIcon")) {
            option = SUPPRESS_ICON;
        }
        else if (!strcasecmp(w, "SuppressLastModified")) {
            option = SUPPRESS_LAST_MOD;
        }
        else if (!strcasecmp(w, "SuppressSize")) {
            option = SUPPRESS_SIZE;
        }
        else if (!strcasecmp(w, "SuppressRules")) {
            option = SUPPRESS_RULES;
        }
        else if (!strcasecmp(w, "TrackModified")) {
            option = TRACK_MODIFIED;
        }
        else if (!strcasecmp(w, "VersionSort")) {
            option = VERSION_SORT;
        }
        else if (!strcasecmp(w, "ShowForbidden")) {
            option = SHOW_FORBIDDEN;
        }
        else if (!strcasecmp(w, "DoJson")) {
            option = DO_JSON_INDEXES;
        }
        else if (!strcasecmp(w, "None")) {
            if (action != '\0') {
                return "Cannot combine '+' or '-' with 'None' keyword";
            }
            opts = NO_OPTIONS;
            opts_add = 0;
            opts_remove = 0;
        }
        else {
            return "Invalid directory indexing option";
        }
        if (action == '\0') {
            opts |= option;
            opts_add = 0;
            opts_remove = 0;
        }
        else if (action == '+') {
            opts_add |= option;
            opts_remove &= ~option;
        }
        else {
            opts_remove |= option;
            opts_add &= ~option;
        }
    }
    if ((opts & NO_OPTIONS) && (opts & ~NO_OPTIONS)) {
        return "Cannot combine other IndexOptions keywords with 'None'";
    }
    d_cfg->incremented_opts = opts_add;
    d_cfg->decremented_opts = opts_remove;
    d_cfg->opts = opts;
    return NULL;
}


static const char *set_default_order(cmd_parms *cmd, void *m,
                                     const char *direction, const char *key)
{
    jsonindex_conf *d_cfg = (jsonindex_conf *) m;

    if (!strcasecmp(direction, "Ascending")) {
        d_cfg->default_direction = D_ASCENDING;
    }
    else if (!strcasecmp(direction, "Descending")) {
        d_cfg->default_direction = D_DESCENDING;
    }
    else {
        return "First keyword must be 'Ascending' or 'Descending'";
    }

    if (!strcasecmp(key, "Name")) {
        d_cfg->default_keyid = K_NAME;
    }
    else if (!strcasecmp(key, "Date")) {
        d_cfg->default_keyid = K_LAST_MOD;
    }
    else if (!strcasecmp(key, "Size")) {
        d_cfg->default_keyid = K_SIZE;
    }
    else if (!strcasecmp(key, "Description")) {
        d_cfg->default_keyid = K_DESC;
    }
    else {
        return "Second keyword must be 'Name', 'Date', 'Size', or "
               "'Description'";
    }

    return NULL;
}

static char *find_item(request_rec *r, apr_array_header_t *list, int path_only)
{
    const char *content_type = ap_field_noparam(r->pool, r->content_type);
    const char *content_encoding = r->content_encoding;
    char *path = r->filename;

    struct item *items = (struct item *) list->elts;
    int i;

    for (i = 0; i < list->nelts; ++i) {
        struct item *p = &items[i];

        /* Special cased for ^^DIRECTORY^^ and ^^BLANKICON^^ */
        if ((path[0] == '^') || (!ap_strcmp_match(path, p->apply_path))) {
            if (!*(p->apply_to)) {
                return p->data;
            }
            else if (p->type == BY_PATH || path[0] == '^') {
                if (!ap_strcmp_match(path, p->apply_to)) {
                    return p->data;
                }
            }
            else if (!path_only) {
                if (!content_encoding) {
                    if (p->type == BY_TYPE) {
                        if (content_type
                            && !ap_strcasecmp_match(content_type,
                                                    p->apply_to)) {
                            return p->data;
                        }
                    }
                }
                else {
                    if (p->type == BY_ENCODING) {
                        if (!ap_strcasecmp_match(content_encoding,
                                                 p->apply_to)) {
                            return p->data;
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

#define find_icon(d,p,t) find_item(p,d->icon_list,t)
#define find_alt(d,p,t) find_item(p,d->alt_list,t)
#define find_header(d,p) find_item(p,d->hdr_list,0)
#define find_readme(d,p) find_item(p,d->rdme_list,0)

static char *find_default_item(char *bogus_name, apr_array_header_t *list)
{
    request_rec r;
    /* Bleah.  I tried to clean up find_item, and it lead to this bit
     * of ugliness.   Note that the fields initialized are precisely
     * those that find_item looks at...
     */
    r.filename = bogus_name;
    r.content_type = r.content_encoding = NULL;
    return find_item(&r, list, 1);
}

#define find_default_icon(d,n) find_default_item(n, d->icon_list)
#define find_default_alt(d,n) find_default_item(n, d->alt_list)

/*
 * Look through the list of pattern/description pairs and return the first one
 * if any) that matches the filename in the request.  If multiple patterns
 * match, only the first one is used; since the order in the array is the
 * same as the order in which directives were processed, earlier matching
 * directives will dominate.
 */

#ifdef CASE_BLIND_FILESYSTEM
#define MATCH_FLAGS APR_FNM_CASE_BLIND
#else
#define MATCH_FLAGS 0
#endif

static char *find_desc(jsonindex_conf *dcfg, const char *filename_full)
{
    int i;
    ai_desc_t *list = (ai_desc_t *) dcfg->desc_list->elts;
    const char *filename_only;
    const char *filename;

    /*
     * If the filename includes a path, extract just the name itself
     * for the simple matches.
     */
    if ((filename_only = ap_strrchr_c(filename_full, '/')) == NULL) {
        filename_only = filename_full;
    }
    else {
        filename_only++;
    }
    for (i = 0; i < dcfg->desc_list->nelts; ++i) {
        ai_desc_t *tuple = &list[i];
        int found;

        /*
         * Only use the full-path filename if the pattern contains '/'s.
         */
        filename = (tuple->full_path) ? filename_full : filename_only;
        /*
         * Make the comparison using the cheapest method; only do
         * wildcard checking if we must.
         */
        if (tuple->wildcards) {
            found = (apr_fnmatch(tuple->pattern, filename, MATCH_FLAGS) == 0);
        }
        else {
            found = (ap_strstr_c(filename, tuple->pattern) != NULL);
        }
        if (found) {
            return tuple->description;
        }
    }
    return NULL;
}

static int ignore_entry(jsonindex_conf *d, char *path)
{
    apr_array_header_t *list = d->ign_list;
    struct item *items = (struct item *) list->elts;
    char *tt;
    int i;

    if ((tt = strrchr(path, '/')) == NULL) {
        tt = path;
    }
    else {
        tt++;
    }

    for (i = 0; i < list->nelts; ++i) {
        struct item *p = &items[i];
        char *ap;

        if ((ap = strrchr(p->apply_to, '/')) == NULL) {
            ap = p->apply_to;
        }
        else {
            ap++;
        }

#ifndef CASE_BLIND_FILESYSTEM
        if (!ap_strcmp_match(path, p->apply_path)
            && !ap_strcmp_match(tt, ap)) {
            return 1;
        }
#else  /* !CASE_BLIND_FILESYSTEM */
        /*
         * On some platforms, the match must be case-blind.  This is really
         * a factor of the filesystem involved, but we can't detect that
         * reliably - so we have to granularise at the OS level.
         */
        if (!ap_strcasecmp_match(path, p->apply_path)
            && !ap_strcasecmp_match(tt, ap)) {
            return 1;
        }
#endif /* !CASE_BLIND_FILESYSTEM */
    }
    return 0;
}


static struct ent *make_parent_entry(apr_int32_t autoindex_opts,
                                     jsonindex_conf *d,
                                     request_rec *r, char keyid,
                                     char direction)
{
    struct ent *p = (struct ent *) apr_pcalloc(r->pool, sizeof(struct ent));
    char *testpath;
    /*
     * p->name is now the true parent URI.
     * testpath is a crafted lie, so that the syntax '/some/..'
     * (or simply '..')be used to describe 'up' from '/some/'
     * when processeing IndexIgnore, and Icon|Alt|Desc configs.
     */

    /* The output has always been to the parent.  Don't make ourself
     * our own parent (worthless cyclical reference).
     */
    if (!(p->name = ap_make_full_path(r->pool, r->uri, "../"))) {
        return (NULL);
    }
    ap_getparents(p->name);
    if (!*p->name) {
        return (NULL);
    }

    /* IndexIgnore has always compared "/thispath/.." */
    testpath = ap_make_full_path(r->pool, r->filename, "..");
    if (ignore_entry(d, testpath)) {
        return (NULL);
    }

    p->size = -1;
    p->lm = -1;
    p->key = apr_toupper(keyid);
    p->ascending = (apr_toupper(direction) == D_ASCENDING);
    p->version_sort = autoindex_opts & VERSION_SORT;
    if (!(p->icon = find_default_icon(d, testpath))) {
        p->icon = find_default_icon(d, "^^DIRECTORY^^");
    }
    if (!(p->alt = find_default_alt(d, testpath))) {
        if (!(p->alt = find_default_alt(d, "^^DIRECTORY^^"))) {
            p->alt = "DIR";
        }
    }
    p->desc = find_desc(d, testpath);
    return p;
}

static struct ent *make_autoindex_entry(const apr_finfo_t *dirent,
                                        int autoindex_opts,
                                        jsonindex_conf *d,
                                        request_rec *r, char keyid,
                                        char direction,
                                        const char *pattern)
{
    request_rec *rr;
    struct ent *p;
    int show_forbidden = 0;

    /* Dot is ignored, Parent is handled by make_parent_entry() */
    if ((dirent->name[0] == '.') && (!dirent->name[1]
        || ((dirent->name[1] == '.') && !dirent->name[2])))
        return (NULL);

    /*
     * On some platforms, the match must be case-blind.  This is really
     * a factor of the filesystem involved, but we can't detect that
     * reliably - so we have to granularise at the OS level.
     */
    if (pattern && (apr_fnmatch(pattern, dirent->name,
                                APR_FNM_NOESCAPE | APR_FNM_PERIOD
#ifdef CASE_BLIND_FILESYSTEM
                                | APR_FNM_CASE_BLIND
#endif
                                )
                    != APR_SUCCESS)) {
        return (NULL);
    }

    if (ignore_entry(d, ap_make_full_path(r->pool,
                                          r->filename, dirent->name))) {
        return (NULL);
    }

    if (!(rr = ap_sub_req_lookup_dirent(dirent, r, AP_SUBREQ_NO_ARGS, NULL))) {
        return (NULL);
    }

    if((autoindex_opts & SHOW_FORBIDDEN)
        && (rr->status == HTTP_UNAUTHORIZED || rr->status == HTTP_FORBIDDEN)) {
        show_forbidden = 1;
    }

    if ((rr->finfo.filetype != APR_DIR && rr->finfo.filetype != APR_REG)
        || !(rr->status == OK || ap_is_HTTP_SUCCESS(rr->status)
                              || ap_is_HTTP_REDIRECT(rr->status)
                              || show_forbidden == 1)) {
        ap_destroy_sub_req(rr);
        return (NULL);
    }

    p = (struct ent *) apr_pcalloc(r->pool, sizeof(struct ent));
    if (dirent->filetype == APR_DIR) {
        p->name = apr_pstrcat(r->pool, dirent->name, "/", NULL);
    }
    else {
        p->name = apr_pstrdup(r->pool, dirent->name);
    }
    p->size = -1;
    p->icon = NULL;
    p->alt = NULL;
    p->desc = NULL;
    p->lm = -1;
    p->isdir = 0;
    p->key = apr_toupper(keyid);
    p->ascending = (apr_toupper(direction) == D_ASCENDING);
    p->version_sort = !!(autoindex_opts & VERSION_SORT);
    p->ignore_case = !!(autoindex_opts & IGNORE_CASE);

        p->lm = rr->finfo.mtime;
        if (dirent->filetype == APR_DIR) {
            if (autoindex_opts & FOLDERS_FIRST) {
                p->isdir = 1;
            }
            rr->filename = ap_make_dirstr_parent (rr->pool, rr->filename);

            /* omit the trailing slash (1.3 compat) */
            rr->filename[strlen(rr->filename) - 1] = '\0';

            if (!(p->icon = find_icon(d, rr, 1))) {
                p->icon = find_default_icon(d, "^^DIRECTORY^^");
            }
            if (!(p->alt = find_alt(d, rr, 1))) {
                if (!(p->alt = find_default_alt(d, "^^DIRECTORY^^"))) {
                    p->alt = "DIR";
                }
            }
        }
        else {
            p->icon = find_icon(d, rr, 0);
            p->alt = find_alt(d, rr, 0);
            p->size = rr->finfo.size;
        }

        p->desc = find_desc(d, rr->filename);

    ap_destroy_sub_req(rr);
    /*
     * We don't need to take any special action for the file size key.
     * If we did, it would go here.
     */
    if (keyid == K_LAST_MOD) {
        if (p->lm < 0) {
            p->lm = 0;
        }
    }
    return (p);
}

static void output_directories(struct ent **ar, int n,
                               jsonindex_conf *d, request_rec *r,
                               apr_int32_t autoindex_opts, char keyid,
                               char direction, const char *colargs)
{
    int x;
    apr_size_t rv;
    apr_pool_t *scratch;
    apr_pool_create(&scratch, r->pool);
    ap_rputs(" \"files\" : [\n", r);
    for (x = 0; x < n; x++) {
        if (x > 0) {
          ap_rputs(",\n", r);
        }

        char *anchor, *t, *t2;

        apr_pool_clear(scratch);

        t = ar[x]->name;
        anchor = escapen_json(r->pool, t, 1892);

        if (!x && t[0] == '/') {
            t2 = "Parent Directory";
            ar[x]->isdir=1; // hmm why do I have to do this here
        }
        else {
            t2 = t;
        }


        if (autoindex_opts) {

            ap_rvputs(r, "  {\"dir\":", ar[x]->isdir == 1 ? "true" : "false" , "," , NULL);
            ap_rvputs(r, "\"name\":\"",  anchor, "\"", NULL);

            if (!(autoindex_opts & SUPPRESS_ICON)) {

                if ( (ar[x]->icon) || d->default_icon ) {
                    ap_rvputs(r,
                              ",\"icon\":\"",
                              escapen_json(r->pool, (ar[x]->icon ? ar[x]->icon : d->default_icon), 1892),
                              "\"",
                              NULL);
                }
            }

            if (!(autoindex_opts & SUPPRESS_LAST_MOD)) {
                if (ar[x]->lm != -1) {
                    char time_str[MAX_STRING_LEN];
                    apr_time_exp_t ts;
                    apr_time_exp_lt(&ts, ar[x]->lm);
                    apr_strftime(time_str, &rv, MAX_STRING_LEN,
                                "%Y-%m-%d %H:%M", &ts);
                    ap_rputs(",\"last_mod\":\"", r);
                    ap_rputs(time_str, r);
                    ap_rputs("\"", r);
                }
            }
            if (!(autoindex_opts & SUPPRESS_SIZE)) {
                ap_rputs(",\"size\":", r);
                ap_rputs(apr_itoa(r->pool, ar[x]->size), r);
            }
            if (!(autoindex_opts & SUPPRESS_DESC)) {
                if (ar[x]->desc) {
                    ap_rputs(",\"desc\":\"", r);
                    ap_rputs(escapen_json(r->pool, ar[x]->desc, 256), r);
                    ap_rputs("\"", r);
                }
            }
            ap_rputs("}", r);
        }
        else {
            ap_rvputs(r, "  { \"", ar[x]->isdir == 1 ? "dir" : "file" , "\" : \"", anchor, "\" }", NULL);
        }
    }
    ap_rputs("\n ]", r);
}

/*
 * Compare two file entries according to the sort criteria.  The return
 * is essentially a signum function value.
 */

static int dsortf(struct ent **e1, struct ent **e2)
{
    struct ent *c1;
    struct ent *c2;
    int result = 0;

    /*
     * First, see if either of the entries is for the parent directory.
     * If so, that *always* sorts lower than anything else.
     */
    if ((*e1)->name[0] == '/') {
        return -1;
    }
    if ((*e2)->name[0] == '/') {
        return 1;
    }
    /*
     * Now see if one's a directory and one isn't, if we're set
     * isdir for FOLDERS_FIRST.
     */
    if ((*e1)->isdir != (*e2)->isdir) {
        return (*e1)->isdir ? -1 : 1;
    }
    /*
     * All of our comparisons will be of the c1 entry against the c2 one,
     * so assign them appropriately to take care of the ordering.
     */
    if ((*e1)->ascending) {
        c1 = *e1;
        c2 = *e2;
    }
    else {
        c1 = *e2;
        c2 = *e1;
    }

    switch (c1->key) {
    case K_LAST_MOD:
        if (c1->lm > c2->lm) {
            return 1;
        }
        else if (c1->lm < c2->lm) {
            return -1;
        }
        break;
    case K_SIZE:
        if (c1->size > c2->size) {
            return 1;
        }
        else if (c1->size < c2->size) {
            return -1;
        }
        break;
    case K_DESC:
        if (c1->version_sort) {
            result = apr_strnatcmp(c1->desc ? c1->desc : "",
                                   c2->desc ? c2->desc : "");
        }
        else {
            result = strcmp(c1->desc ? c1->desc : "",
                            c2->desc ? c2->desc : "");
        }
        if (result) {
            return result;
        }
        break;
    }

    /* names may identical when treated case-insensitively,
     * so always fall back on strcmp() flavors to put entries
     * in deterministic order.  This means that 'ABC' and 'abc'
     * will always appear in the same order, rather than
     * variably between 'ABC abc' and 'abc ABC' order.
     */

    if (c1->version_sort) {
        if (c1->ignore_case) {
            result = apr_strnatcasecmp (c1->name, c2->name);
        }
        if (!result) {
            result = apr_strnatcmp(c1->name, c2->name);
        }
    }

    /* The names may be identical in respects other other than
     * filename case when strnatcmp is used above, so fall back
     * to strcmp on conflicts so that fn1.01.zzz and fn1.1.zzz
     * are also sorted in a deterministic order.
     */

    if (!result && c1->ignore_case) {
        result = strcasecmp (c1->name, c2->name);
    }

    if (!result) {
        result = strcmp (c1->name, c2->name);
    }

    return result;
}


// RESPONSES

static void emit_head(request_rec *r, char *title) {
  ap_rputs("{\n \"path\" : \"", r);
  ap_rputs(title, r);
  ap_rputs("\",\n", r);
}
static void emit_tail(request_rec *r) {
  ap_rputs(" \n}", r);
}
static int emit_file(request_rec *r) {
  jsonindex_conf *conf = (jsonindex_conf *)ap_get_module_config(r->per_dir_config,
                                                      &jsonindex_module);
  char *name = conf->json_index_file;
  if ( name[0] == 0 ) {
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "JsonIndexFile not configured");
    return HTTP_INTERNAL_SERVER_ERROR;
  }
  int res;
  apr_file_t *f;
  apr_status_t fstatus;
  apr_finfo_t fi;

  apr_fileperms_t perm = APR_OS_DEFAULT;
  fstatus = apr_file_open(&f, name, APR_READ, perm, r->pool);
  if (fstatus != APR_SUCCESS) {
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "mod_jsonindex incorrectly configured, can not open %s", name);
    return HTTP_FORBIDDEN;
  }

  if (f == NULL) {
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "mod_jsonindex incorrectly configured, can not access %s", name);
    return HTTP_FORBIDDEN;
  }

  if ((res = apr_stat(&fi, name, APR_FINFO_SIZE, r->pool)) != APR_SUCCESS) {
    ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "mod_jsonindex incorrectly configured, can not stat %s", name);
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


static int json_indexes(request_rec *r, jsonindex_conf *d) {
    char *title_name = escapen_json(r->pool, r->uri, strlen(r->uri));
    char *title_endp;
    char *name = r->filename;
    char *pstring = NULL;
    apr_finfo_t dirent;
    apr_dir_t *thedir;
    apr_status_t status;
    int num_ent = 0, x;
    struct ent *head, *p;
    struct ent **ar = NULL;
    const char *qstring;
    apr_int32_t jsonindex_opts = d->opts;
    char keyid;
    char direction;
    char *colargs;
    char *fullpath;
    apr_size_t dirpathlen;
    char *ctype = "application/json;charset=UTF-8";

    if ((status = apr_dir_open(&thedir, name, r->pool)) != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r,
                      "Can't open directory for index: %s", r->filename);
        return HTTP_FORBIDDEN;
    }

    ap_set_content_type(r, ctype);

    if (jsonindex_opts & TRACK_MODIFIED) {
        ap_update_mtime(r, r->finfo.mtime);
        ap_set_last_modified(r);
        ap_set_etag(r);
    }
    if (r->header_only) {
        apr_dir_close(thedir);
        return 0;
    }

    /*
     * If there is no specific ordering defined for this directory,
     * default to ascending by filename.
     */
    keyid = d->default_keyid
                ? d->default_keyid : K_NAME;
    direction = d->default_direction
                ? d->default_direction : D_ASCENDING;

    /*
     * Figure out what sort of indexing (if any) we're supposed to use.
     *
     * If no QUERY_STRING was specified or client query strings have been
     * explicitly disabled.
     * If we are ignoring the client, suppress column sorting as well.
     */
    if (jsonindex_opts & IGNORE_CLIENT) {
        qstring = NULL;
        jsonindex_opts |= SUPPRESS_COLSORT;
        colargs = "";
    }
    else {
        char fval[5], vval[5], *ppre = "", *epattern = "";
        fval[0] = '\0'; vval[0] = '\0';
        qstring = r->args;

        while (qstring && *qstring) {

            /* C= First Sort key Column (N, M, S, D) */
            if (   qstring[0] == 'C' && qstring[1] == '='
                && qstring[2] && strchr(K_VALID, qstring[2])
                && (   qstring[3] == '&' || qstring[3] == ';'
                    || !qstring[3])) {
                keyid = qstring[2];
                qstring += qstring[3] ? 4 : 3;
            }

            /* O= Sort order (A, D) */
            else if (   qstring[0] == 'O' && qstring[1] == '='
                     && (   (qstring[2] == D_ASCENDING)
                         || (qstring[2] == D_DESCENDING))
                     && (   qstring[3] == '&' || qstring[3] == ';'
                         || !qstring[3])) {
                direction = qstring[2];
                qstring += qstring[3] ? 4 : 3;
            }

            /* V= Version sort (0, 1) */
            else if (   qstring[0] == 'V' && qstring[1] == '='
                     && (qstring[2] == '0' || qstring[2] == '1')
                     && (   qstring[3] == '&' || qstring[3] == ';'
                         || !qstring[3])) {
                if (qstring[2] == '0') {
                    jsonindex_opts &= ~VERSION_SORT;
                }
                else if (qstring[2] == '1') {
                    jsonindex_opts |= VERSION_SORT;
                }
                strcpy(vval, ";V= ");
                vval[3] = qstring[2];
                qstring += qstring[3] ? 4 : 3;
            }

            /* P= wildcard pattern (*.foo) */
            else if (qstring[0] == 'P' && qstring[1] == '=') {
                const char *eos = qstring += 2; /* for efficiency */

                while (*eos && *eos != '&' && *eos != ';') {
                    ++eos;
                }

                if (eos == qstring) {
                    pstring = NULL;
                }
                else {
                    pstring = apr_pstrndup(r->pool, qstring, eos - qstring);
                    if (ap_unescape_url(pstring) != OK) {
                        /* ignore the pattern, if it's bad. */
                        pstring = NULL;
                    }
                    else {
                        ppre = ";P=";
                        /* be correct */
                        epattern = ap_escape_uri(r->pool, pstring);
                    }
                }

                if (*eos && *++eos) {
                    qstring = eos;
                }
                else {
                    qstring = NULL;
                }
            }

            /* Syntax error?  Ignore the remainder! */
            else {
                qstring = NULL;
            }
        }
        colargs = apr_pstrcat(r->pool, fval, vval, ppre, epattern, NULL);
    }
    /* Spew preamble */
    title_endp = title_name + strlen(title_name) - 1;

    while (title_endp > title_name && *title_endp == '/') {
        *title_endp-- = '\0';
    }
    emit_head(r, title_name);

    /*
     * Since we don't know how many dir. entries there are, put them into a
     * linked list and then arrayificate them so qsort can use them.
     */
    head = NULL;
    p = make_parent_entry(jsonindex_opts, d, r, keyid, direction);
    if (p != NULL) {
        p->next = head;
        head = p;
        num_ent++;
    }
    fullpath = apr_palloc(r->pool, APR_PATH_MAX);
    dirpathlen = strlen(name);
    memcpy(fullpath, name, dirpathlen);

    do {
        status = apr_dir_read(&dirent, APR_FINFO_MIN | APR_FINFO_NAME, thedir);
        if (APR_STATUS_IS_INCOMPLETE(status)) {
            continue; /* ignore un-stat()able files */
        }
        else if (status != APR_SUCCESS) {
            break;
        }

        /* We want to explode symlinks here. */
        if (dirent.filetype == APR_LNK) {
            const char *savename;
            apr_finfo_t fi;
            /* We *must* have FNAME. */
            savename = dirent.name;
            apr_cpystrn(fullpath + dirpathlen, dirent.name,
                        APR_PATH_MAX - dirpathlen);
            status = apr_stat(&fi, fullpath,
                              dirent.valid & ~(APR_FINFO_NAME), r->pool);
            if (status != APR_SUCCESS) {
                /* Something bad happened, skip this file. */
                continue;
            }
            memcpy(&dirent, &fi, sizeof(fi));
            dirent.name = savename;
            dirent.valid |= APR_FINFO_NAME;
        }
        p = make_autoindex_entry(&dirent, jsonindex_opts, d, r,
                                 keyid, direction, pstring);
        if (p != NULL) {
            p->next = head;
            head = p;
            num_ent++;
        }
    } while (1);
    if (num_ent > 0) {
        ar = (struct ent **) apr_palloc(r->pool,
                                        num_ent * sizeof(struct ent *));
        p = head;
        x = 0;
        while (p) {
            ar[x++] = p;
            p = p->next;
        }

        qsort((void *) ar, num_ent, sizeof(struct ent *),
              (int (*)(const void *, const void *)) dsortf);
    }
    output_directories(ar, num_ent, d, r, jsonindex_opts,
                       keyid, direction, colargs);
    apr_dir_close(thedir);

    emit_tail(r);

    return OK;
}

// HANDLER
static int jsonindex_handler(request_rec *r) {
  // handlers are always called so bomb out early if we are not handling this request

  if(strcmp(r->handler,DIR_MAGIC_TYPE)) {
    return DECLINED;
  }

  // be specific about the requires type to handle
  if (strcmp( r->method, "GET" ) != 0 ) {
    return DECLINED;
  }

  // handler code
  jsonindex_conf *d;
  int allow_opts;
  const char* x_json_index;
  const char* param_json_index;

  allow_opts = ap_allow_options(r);

  d = (jsonindex_conf *) ap_get_module_config(r->per_dir_config,
                                                      &jsonindex_module);
  if ( ! (d->opts & DO_JSON_INDEXES)) {
    return DECLINED;
  }

  r->allowed |= (AP_METHOD_BIT << M_GET);
  if (r->method_number != M_GET) {
    return DECLINED;
  }

  param_json_index = r->args;
  ap_log_perror(APLOG_MARK, 1, APR_SUCCESS, r->pool, "param_json_index [%s]", param_json_index);

// prevent other sites from downloading the indexes
// TODO configure the requirement for a header
  x_json_index = apr_table_get(r->headers_in, "X-JsonIndex");
  if (x_json_index == NULL) {
    emit_file(r);
    return OK;
  }


  if ( param_json_index != NULL && strncmp(param_json_index, "json=true", 9) == 0 ) {
    /* OK, nothing easy.  Trot out the heavy artillery... */
    int errstatus;

    if ((errstatus = ap_discard_request_body(r)) != OK) {
      return errstatus;
    }

    /* KLUDGE --- make the sub_req lookups happen in the right directory.
     * Fixing this in the sub_req_lookup functions themselves is difficult,
     * and would probably break virtual includes...
     */

    if (r->filename[strlen(r->filename) - 1] != '/') {
      r->filename = apr_pstrcat(r->pool, r->filename, "/", NULL);
    }

    return json_indexes(r, d);
  }
  return DECLINED;
}

// STANDARD APACHE MODULE STUFF

#define DIR_CMD_PERMS OR_INDEXES

static void *create_jsonindex_config(apr_pool_t *p, char *dummy)
{
    jsonindex_conf *new = (jsonindex_conf *)apr_pcalloc(p, sizeof(jsonindex_conf));

    new->json_index_file = '\0';
    new->default_icon = '\0';
    new->icon_list = apr_array_make(p, 4, sizeof(struct item));
    new->alt_list = apr_array_make(p, 4, sizeof(struct item));
    new->desc_list = apr_array_make(p, 4, sizeof(ai_desc_t));
    new->ign_list = apr_array_make(p, 4, sizeof(struct item));
    new->hdr_list = apr_array_make(p, 4, sizeof(struct item));
    new->rdme_list = apr_array_make(p, 4, sizeof(struct item));
    new->opts = 0;
    new->incremented_opts = 0;
    new->decremented_opts = 0;
    new->default_keyid = '\0';
    new->default_direction = '\0';
    return (void *) new;
}

static void *merge_jsonindex_configs(apr_pool_t *p, void *basev, void *addv)
{
    jsonindex_conf *new;
    jsonindex_conf *base = (jsonindex_conf *) basev;
    jsonindex_conf *add = (jsonindex_conf *) addv;

    new = (jsonindex_conf *) apr_pcalloc(p, sizeof(jsonindex_conf));
    new->default_icon = add->default_icon ? add->default_icon
                                          : base->default_icon;
    new->json_index_file = add->json_index_file ? add->json_index_file
                                          : base->json_index_file;
    new->alt_list = apr_array_append(p, add->alt_list, base->alt_list);
    new->ign_list = apr_array_append(p, add->ign_list, base->ign_list);
    new->hdr_list = apr_array_append(p, add->hdr_list, base->hdr_list);
    new->desc_list = apr_array_append(p, add->desc_list, base->desc_list);
    new->icon_list = apr_array_append(p, add->icon_list, base->icon_list);
    new->rdme_list = apr_array_append(p, add->rdme_list, base->rdme_list);
    if (add->opts & NO_OPTIONS) {
        /*
         * If the current directory says 'no options' then we also
         * clear any incremental mods from being inheritable further down.
         */
        new->opts = NO_OPTIONS;
        new->incremented_opts = 0;
        new->decremented_opts = 0;
    }
    else {
        /*
         * If there were any nonincremental options selected for
         * this directory, they dominate and we don't inherit *anything.*
         * Contrariwise, we *do* inherit if the only settings here are
         * incremental ones.
         */
        if (add->opts == 0) {
            new->incremented_opts = (base->incremented_opts
                                     | add->incremented_opts)
                                    & ~add->decremented_opts;
            new->decremented_opts = (base->decremented_opts
                                     | add->decremented_opts);
            /*
             * We may have incremental settings, so make sure we don't
             * inadvertently inherit an IndexOptions None from above.
             */
            new->opts = (base->opts & ~NO_OPTIONS);
        }
        else {
            /*
             * There are local nonincremental settings, which clear
             * all inheritance from above.  They *are* the new base settings.
             */
            new->opts = add->opts;;
        }
        /*
         * We're guaranteed that there'll be no overlap between
         * the add-options and the remove-options.
         */
        new->opts |= new->incremented_opts;
        new->opts &= ~new->decremented_opts;
    }

    new->default_keyid = add->default_keyid ? add->default_keyid
                                            : base->default_keyid;
    new->default_direction = add->default_direction ? add->default_direction
                                                    : base->default_direction;
    return new;
}

static const command_rec jsonindex_cmds[] = {
    AP_INIT_ITERATE2("AddIcon", add_icon, BY_PATH, DIR_CMD_PERMS,
                     "an icon URL followed by one or more filenames"),
    AP_INIT_ITERATE2("AddIconByType", add_icon, BY_TYPE, DIR_CMD_PERMS,
                     "an icon URL followed by one or more MIME types"),
    AP_INIT_ITERATE2("AddIconByEncoding", add_icon, BY_ENCODING, DIR_CMD_PERMS,
                     "an icon URL followed by one or more content encodings"),
    AP_INIT_ITERATE2("AddAlt", add_alt, BY_PATH, DIR_CMD_PERMS,
                     "alternate descriptive text followed by one or more "
                     "filenames"),
    AP_INIT_ITERATE2("AddAltByType", add_alt, BY_TYPE, DIR_CMD_PERMS,
                     "alternate descriptive text followed by one or more MIME "
                     "types"),
    AP_INIT_ITERATE2("AddAltByEncoding", add_alt, BY_ENCODING, DIR_CMD_PERMS,
                     "alternate descriptive text followed by one or more "
                     "content encodings"),
    AP_INIT_TAKE_ARGV("JsonIndexOptions", add_opts, NULL, DIR_CMD_PERMS,
                      "one or more index options [+|-][]"),
    AP_INIT_TAKE2("JsonIndexOrderDefault", set_default_order, NULL, DIR_CMD_PERMS,
                  "{Ascending,Descending} {Name,Size,Description,Date}"),
    AP_INIT_ITERATE("JsonIndexIgnore", add_ignore, NULL, DIR_CMD_PERMS,
                    "one or more file extensions"),
    AP_INIT_ITERATE2("AddDescription", add_desc, BY_PATH, DIR_CMD_PERMS,
                     "Descriptive text followed by one or more filenames"),
    AP_INIT_TAKE1("HeaderName", add_header, NULL, DIR_CMD_PERMS,
                  "a filename"),
    AP_INIT_TAKE1("JsonIndexFile", add_json_index_file, NULL, DIR_CMD_PERMS,
                  "file name of the index html"),
    AP_INIT_TAKE1("ReadmeName", add_readme, NULL, DIR_CMD_PERMS,
                  "a filename"),
    AP_INIT_TAKE1("DefaultIcon", ap_set_string_slot,
                  (void *)APR_OFFSETOF(jsonindex_conf, default_icon),
                  DIR_CMD_PERMS, "an icon URL"),
    {NULL}
} ;

/* Make the name of the content handler known to Apache */
static void jsonindex_hooks(apr_pool_t *p)
{
  // This is the call to make to register a handler for method calls (GET PUT et. al.).
  ap_hook_handler(jsonindex_handler, NULL, NULL, APR_HOOK_FIRST);
}


module AP_MODULE_DECLARE_DATA jsonindex_module = {
  STANDARD20_MODULE_STUFF,
  create_jsonindex_config,
  merge_jsonindex_configs,
  NULL,
  NULL,
  jsonindex_cmds,
  jsonindex_hooks
} ;

