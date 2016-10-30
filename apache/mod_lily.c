/*  mod_lily.c
    This is an apache binding for the Lily language. */
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"
#include "util_script.h"

#include "lily_utf8.h"
#include "lily_parser.h"

#include "lily_api_alloc.h"
#include "lily_api_embed.h"
#include "lily_api_value.h"

typedef struct {
    int show_traceback;
} lily_config_rec;

module AP_MODULE_DECLARE_DATA lily_module;

/**
embedded server

This package is registered when Lily is run by Apache through mod_lily. This
package provides Lily with information inside of Apache (such as POST), as well
as functions for sending data through the Apache server.
*/
lily_value *bind_tainted_of(lily_string_val *input)
{
    lily_instance_val *iv = lily_new_instance_val(1);
    lily_instance_set_string(iv, 0, input);
    return lily_new_value_of_instance(LILY_TAINTED_ID, iv);
}

static void add_hash_entry(lily_hash_val *hash_val, lily_string_val *key,
        lily_string_val *record)
{
    lily_instance_val *tainted_rec = lily_new_instance_val(1);
    lily_instance_set_string(tainted_rec, 0, record);
    lily_value *boxed_rec = lily_new_value_of_instance(LILY_TAINTED_ID,
            tainted_rec);

    lily_hash_insert_str(hash_val, key, boxed_rec);

    /* new_value and insert both add a ref, which is one too many. Fix that. */
    lily_deref(boxed_rec);
}

static int bind_table_entry(void *data, const char *key, const char *value)
{
    /* Don't allow anything to become a string that has invalid utf-8, because
       Lily's string type assumes valid utf-8. */
    if (lily_is_valid_utf8(key) == 0 ||
        lily_is_valid_utf8(value) == 0)
        return TRUE;

    lily_hash_val *hash_val = (lily_hash_val *)data;

    lily_string_val *string_key = lily_new_raw_string(key);
    lily_string_val *record = lily_new_raw_string(value);

    add_hash_entry(hash_val, string_key, record);
    return TRUE;
}

static lily_value *bind_table_as(lily_options *options, apr_table_t *table,
        char *name)
{
    lily_hash_val *hv = lily_new_hash_strtable();
    apr_table_do(bind_table_entry, hv, table, NULL);
    return lily_new_value_of_hash(hv);
}

/**
var env: Hash[String, Tainted[String]]

This contains key+value pairs containing the current environment of the server.
*/
static lily_value *load_var_env(lily_options *options, uint16_t *unused)
{
    request_rec *r = (request_rec *)options->data;
    ap_add_cgi_vars(r);
    ap_add_common_vars(r);

    return bind_table_as(options, r->subprocess_env, "env");
}

/**
var get: Hash[String, Tainted[String]]

This contains key+value pairs that were sent to the server as GET variables.
Any pair that has a key or a value that is not valid utf-8 will not be present.
*/
static lily_value *load_var_get(lily_options *options, uint16_t *unused)
{
    apr_table_t *http_get_args;
    ap_args_to_table((request_rec *)options->data, &http_get_args);

    return bind_table_as(options, http_get_args, "get");
}

/**
var httpmethod: String

This is the method that was used to make the request to the server.
Common values are "GET", and "POST".
*/
static lily_value *load_var_httpmethod(lily_options *options, uint16_t *unused)
{
    request_rec *r = (request_rec *)options->data;

    return lily_new_value_of_string(lily_new_raw_string(r->method));
}

/**
var post: Hash[String, Tainted[String]]

This contains key+value pairs that were sent to the server as POST variables.
Any pair that has a key or a value that is not valid utf-8 will not be present.
*/
static lily_value *load_var_post(lily_options *options, uint16_t *unused)
{
    lily_hash_val *hv = lily_new_hash_strtable();
    request_rec *r = (request_rec *)options->data;

    apr_array_header_t *pairs;
    apr_off_t len;
    apr_size_t size;
    char *buffer;

    /* Credit: I found out how to use this by reading httpd 2.4's mod_lua
       (specifically req_parsebody of lua_request.c). */
    int res = ap_parse_form_data(r, NULL, &pairs, -1, 1024 * 8);
    if (res == OK) {
        while (pairs && !apr_is_empty_array(pairs)) {
            ap_form_pair_t *pair = (ap_form_pair_t *) apr_array_pop(pairs);
            if (lily_is_valid_utf8(pair->name) == 0)
                continue;

            apr_brigade_length(pair->value, 1, &len);
            size = (apr_size_t) len;
            buffer = lily_malloc(size + 1);

            if (lily_is_valid_utf8(buffer) == 0) {
                lily_free(buffer);
                continue;
            }

            apr_brigade_flatten(pair->value, buffer, &size);
            buffer[len] = 0;

            lily_string_val *key = lily_new_raw_string(pair->name);
            /* Give the buffer to the value to save memory. */
            lily_string_val *record = lily_new_raw_string_take(buffer);

            add_hash_entry(hv, key, record);
        }
    }

    return lily_new_value_of_hash(hv);
}

extern void lily_builtin_String_html_encode(lily_state *);
extern int lily_maybe_html_encode_to_buffer(lily_state *, lily_value *);

/**
define escape(text: String): String

This checks self for having `"&"`, `"<"`, or `">"`. If any are found, then a new
String is created where those html entities are replaced (`"&"` becomes
`"&amp;"`, `"<"` becomes `"&lt;"`, `">"` becomes `"&gt;"`).
*/
void lily_server_escape(lily_state *s)
{
    lily_builtin_String_html_encode(s);
}

/**
define write(text: String)

This escapes, then writes 'text' to the server. It is equivalent to
`server.write_raw(server.escape(text))`, except faster because it skips building
an intermediate `String` value.
*/
void lily_server_write(lily_state *s)
{
    lily_value *input = lily_arg_value(s, 0);
    const char *source;

    /* String.html_encode can't be called directly, for a couple reasons.
       1: It expects a result register, and there isn't one.
       2: It may create a new String, which is unnecessary. */
    if (lily_maybe_html_encode_to_buffer(s, input) == 0)
        source = input->value.string->string;
    else
        source = lily_mb_get(lily_get_msgbuf_noflush(s));

    ap_rputs(source, (request_rec *)lily_get_data(s));
}

/**
define write_literal(text: String)

This writes `text` directly to the server. If `text` is not a `String` literal,
then `ValueError` is raised. No escaping is performed.
*/
void lily_server_write_literal(lily_state *s)
{
    lily_value *write_reg = lily_arg_value(s, 0);

    if (lily_value_is_derefable(write_reg) == 0)
        lily_ValueError(s, "The string passed must be a literal.\n");

    char *value = write_reg->value.string->string;

    ap_rputs(value, (request_rec *)s->data);
}

/**
define write_raw(text: String)

This writes `text` directly to the server without performing any HTML character
escaping. Use this only if you are certain that there is no possibility of HTML
injection.
*/
void lily_server_write_raw(lily_state *s)
{
    char *value = lily_arg_string_raw(s, 0);

    ap_rputs(value, (request_rec *)s->data);
}

#include "dyna_server.h"

static int lily_handler(request_rec *r)
{
    if (r->handler == NULL || strcmp(r->handler, "lily"))
        return DECLINED;

    r->content_type = "text/html";

    lily_config_rec *conf = (lily_config_rec *)ap_get_module_config(
            r->per_dir_config, &lily_module);

    lily_options *options = lily_new_default_options();
    options->data = r;
    options->html_sender = (lily_html_sender) ap_rputs;
    options->allow_sys = 0;

    lily_state *state = lily_new_state(options);
    register_server(state);

    int result = lily_exec_template_file(state, r->filename);

    if (result == 0 && conf->show_traceback)
        /* !!!: This is unsafe, because it's possible for the error message to
           have html entities in the message. */
        ap_rputs(lily_get_error(state), r);

    lily_free_state(state);

    return OK;
}

static const char *cmd_showtraceback(cmd_parms *cmd, void *p, int flag)
{
    lily_config_rec *conf = (lily_config_rec *)p;
    conf->show_traceback = flag;

    return NULL;
}

static void *perdir_create(apr_pool_t *p, char *dummy)
{
    lily_config_rec *conf = apr_pcalloc(p, sizeof(lily_config_rec));

    conf->show_traceback = 0;
    return conf;
}

static void *perdir_merge(apr_pool_t *pool, void *a, void *b)
{
    lily_config_rec *add = (lily_config_rec *)b;
    lily_config_rec *conf = apr_palloc(pool, sizeof(lily_config_rec));

    conf->show_traceback = add->show_traceback;

    return conf;
}

static const command_rec command_table[] = {
    AP_INIT_FLAG("ShowTraceback", cmd_showtraceback, NULL, OR_FILEINFO,
            "If On, show interpreter exception traceback. Default: Off."),
    {NULL}
};

static void lily_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(lily_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA lily_module = {
    STANDARD20_MODULE_STUFF,
    perdir_create,         /* create per-dir    config structures */
    perdir_merge,          /* merge  per-dir    config structures */
    NULL,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    command_table,         /* table of config file commands       */
    lily_register_hooks    /* register hooks                      */
};
