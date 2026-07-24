// WebView WebKitGTK Implementation
// This file implements the WebKitGTK integration

#include "webview_webkitgtk.h"
#include "webview_webkitgtk_flutter_settings.h"
#include <flutter_linux/flutter_linux.h>
#include <webkit2/webkit2.h>
#include <jsc/jsc.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <string.h>

typedef struct _ScriptHandlerUserData
{
  WebViewWebKitGTK *instance;
  gchar *handler_name;
} ScriptHandlerUserData;

// Shared WebKitWebContext: URI schemes can only be registered once per context.
// Map each WebKitWebView back to its wrapper so scheme callbacks attribute the
// correct viewId (compose vs reader) even when registration userdata is stale.
static GHashTable *g_webkit_instances_by_view = nullptr;

static void ensure_webkit_instance_table(void)
{
  if (g_webkit_instances_by_view == nullptr)
  {
    g_webkit_instances_by_view =
        g_hash_table_new(g_direct_hash, g_direct_equal);
  }
}

static void register_webkit_instance(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view)
    return;
  ensure_webkit_instance_table();
  g_hash_table_insert(g_webkit_instances_by_view, instance->web_view, instance);
}

static void unregister_webkit_instance(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view || !g_webkit_instances_by_view)
    return;
  g_hash_table_remove(g_webkit_instances_by_view, instance->web_view);
}

static WebViewWebKitGTK *instance_for_scheme_request(
    WebKitURISchemeRequest *request,
    gpointer registration_userdata)
{
  if (request != nullptr)
  {
    WebKitWebView *web_view = webkit_uri_scheme_request_get_web_view(request);
    if (web_view != nullptr && g_webkit_instances_by_view != nullptr)
    {
      WebViewWebKitGTK *found = static_cast<WebViewWebKitGTK *>(
          g_hash_table_lookup(g_webkit_instances_by_view, web_view));
      if (found != nullptr)
        return found;
    }
  }
  return static_cast<WebViewWebKitGTK *>(registration_userdata);
}

static void script_handler_user_data_free(gpointer data)
{
  ScriptHandlerUserData *handler_data = static_cast<ScriptHandlerUserData *>(data);
  if (!handler_data)
  {
    return;
  }
  g_free(handler_data->handler_name);
  g_free(handler_data);
}

static const char kInAppWebViewBridgeScript[] =
    "(function(){"
    "if(window.flutter_inappwebview&&window.flutter_inappwebview.callHandler){return;}"
    "var seq=0,pending={};"
    "function wkPost(name,body){"
    "window.webkit.messageHandlers[name].postMessage(body);"
    "}"
    "window.flutter_inappwebview={"
    "callHandler:function(name){"
    "var args=Array.prototype.slice.call(arguments,1),id=String(++seq);"
    "return new Promise(function(resolve,reject){"
    "pending[id]={resolve:resolve,reject:reject};"
    "try{wkPost(name,JSON.stringify({id:id,args:args}));}"
    "catch(e){delete pending[id];reject(e);}});"
    "},"
    "_complete:function(json,error){"
    "var message=JSON.parse(json),entry=pending[message.id];"
    "if(!entry){return;}delete pending[message.id];"
    "if(error||message.error){entry.reject(new Error(error||message.error));}"
    "else{entry.resolve(message.result);}}"
    "};"
    "window.dispatchEvent(new Event('flutterInAppWebViewPlatformReady'));"
    "})();";

typedef struct _MessageResultContext
{
  WebKitWebView *web_view;
} MessageResultContext;

static void message_result_context_free(MessageResultContext *context)
{
  if (!context)
    return;
  g_object_unref(context->web_view);
  g_free(context);
}

static void on_dart_message_result(GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
  MessageResultContext *context = static_cast<MessageResultContext *>(user_data);
  g_autoptr(GError) error = nullptr;
  g_autoptr(FlMethodResponse) response = fl_method_channel_invoke_method_finish(
      FL_METHOD_CHANNEL(source_object), result, &error);

  const gchar *json = "{\"id\":null,\"result\":null}";
  const gchar *error_message = nullptr;
  if (error)
    error_message = error->message;
  else if (FL_IS_METHOD_SUCCESS_RESPONSE(response))
  {
    FlValue *value = fl_method_success_response_get_result(
        FL_METHOD_SUCCESS_RESPONSE(response));
    if (value && fl_value_get_type(value) == FL_VALUE_TYPE_STRING)
      json = fl_value_get_string(value);
  }
  else
    error_message = "JavaScript handler failed";

  g_autofree gchar *escaped_json = g_strescape(json, nullptr);
  g_autofree gchar *escaped_error = error_message ? g_strescape(error_message, nullptr) : nullptr;
  g_autofree gchar *script = g_strdup_printf(
      "window.flutter_inappwebview&&window.flutter_inappwebview._complete(\"%s\",%s%s%s);",
      escaped_json,
      escaped_error ? "\"" : "null",
      escaped_error ? escaped_error : "",
      escaped_error ? "\"" : "");
  webkit_web_view_evaluate_javascript(context->web_view, script, -1, nullptr,
                                      nullptr, nullptr, nullptr, nullptr);
  message_result_context_free(context);
}

static void webview_webkitgtk_inject_bridge_script(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->user_content_manager)
  {
    return;
  }

  WebKitUserScript *script = webkit_user_script_new(
      kInAppWebViewBridgeScript,
      WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
      WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
      nullptr,
      nullptr);
  webkit_user_content_manager_add_script(instance->user_content_manager, script);
  webkit_user_script_unref(script);
}

static void webview_webkitgtk_send_on_message(
    WebViewWebKitGTK *instance,
    const gchar *handler_name,
    const gchar *payload)
{
  if (!instance || !instance->method_channel || !handler_name)
  {
    return;
  }

  g_autoptr(FlValue) args = fl_value_new_map();
  fl_value_set_string_take(args, "name", fl_value_new_string(handler_name));
  fl_value_set_string_take(
      args, "payload", fl_value_new_string(payload ? payload : ""));
  MessageResultContext *context = g_new0(MessageResultContext, 1);
  context->web_view = WEBKIT_WEB_VIEW(g_object_ref(instance->web_view));
  fl_method_channel_invoke_method(
      instance->method_channel, "onMessage", args, nullptr,
      on_dart_message_result, context);
}

#ifndef WEBVIEW_ENABLE_DEBUG_PRINTS
#define g_print(...) ((void)0)
#endif

// Forward declarations for signal handlers
static void on_load_changed(WebKitWebView *web_view,
                            WebKitLoadEvent load_event,
                            gpointer user_data);

static void on_load_failed(WebKitWebView *web_view,
                           WebKitLoadEvent load_event,
                           const gchar *failing_uri,
                           GError *error,
                           gpointer user_data);

static void on_script_message_for_handler(WebKitUserContentManager *manager,
                                            WebKitJavascriptResult *js_result,
                                            gpointer user_data);

// Forward declaration for scheme request callback
static void custom_scheme_request_callback(WebKitURISchemeRequest *request,
                                           gpointer user_data);

// Helper function to free scheme content
static void scheme_content_free(gpointer data)
{
  WebViewSchemeContent *content =
      (WebViewSchemeContent *)data;
  if (content)
  {
    g_free(content->content);
    g_free(content->content_type);
    g_free(content);
  }
}

typedef struct _CustomSchemeRequestContext
{
  WebKitURISchemeRequest *request;
  WebViewWebKitGTK *instance;
} CustomSchemeRequestContext;

static void custom_scheme_request_context_free(CustomSchemeRequestContext *context)
{
  if (!context)
    return;
  if (context->request)
    g_object_unref(context->request);
  g_free(context);
}

static gboolean finish_scheme_from_native_route(WebViewWebKitGTK *instance,
                                                WebKitURISchemeRequest *request,
                                                const gchar *uri)
{
  if (!instance || !instance->scheme_routes || !uri)
    return FALSE;

  const gchar *path = uri;
  const gchar *scheme_sep = strstr(uri, "://");
  if (scheme_sep != nullptr)
  {
    path = scheme_sep + 3;
    // Skip authority host (e.g. "local") when present: appmsg://local/foo -> /foo
    const gchar *slash = strchr(path, '/');
    if (slash != nullptr)
    {
      path = slash;
    }
    else
    {
      path = "/";
    }
  }
  if (*path == '/')
  {
    path++;
  }

  // Strip query/fragment so compose/shell.html?v=53 matches compose/shell.html.
  g_autofree gchar *path_owned = nullptr;
  const gchar *q = strchr(path, '?');
  const gchar *hash = strchr(path, '#');
  const gchar *cut = nullptr;
  if (q != nullptr && hash != nullptr)
    cut = (q < hash) ? q : hash;
  else if (q != nullptr)
    cut = q;
  else if (hash != nullptr)
    cut = hash;
  if (cut != nullptr)
  {
    path_owned = g_strndup(path, (gsize)(cut - path));
    path = path_owned;
  }

  WebViewSchemeContent *content =
      (WebViewSchemeContent *)g_hash_table_lookup(instance->scheme_routes, path);
  if (!content && path[0] != '\0')
  {
    // Also try with leading slash preserved as key variants.
    g_autofree gchar *with_slash = g_strdup_printf("/%s", path);
    content = (WebViewSchemeContent *)g_hash_table_lookup(
        instance->scheme_routes, with_slash);
  }

  if (!content)
  {
    return FALSE;
  }

  GInputStream *stream = g_memory_input_stream_new_from_data(
      content->content, content->content_length, nullptr);
  WebKitURISchemeResponse *response =
      webkit_uri_scheme_response_new(stream, content->content_length);
  webkit_uri_scheme_response_set_content_type(response, content->content_type);
  webkit_uri_scheme_request_finish_with_response(request, response);
  g_object_unref(stream);
  g_object_unref(response);
  g_print("🐧 Scheme request served from native route: %s (%zu bytes, %s)\n",
          path, content->content_length, content->content_type);
  return TRUE;
}

static void on_dart_custom_scheme_result(GObject *source_object,
                                         GAsyncResult *result,
                                         gpointer user_data)
{
  CustomSchemeRequestContext *context =
      static_cast<CustomSchemeRequestContext *>(user_data);
  g_autoptr(GError) error = nullptr;
  g_autoptr(FlMethodResponse) response = fl_method_channel_invoke_method_finish(
      FL_METHOD_CHANNEL(source_object), result, &error);

  WebKitURISchemeRequest *request = context->request;
  WebViewWebKitGTK *instance = context->instance;

  if (error || !FL_IS_METHOD_SUCCESS_RESPONSE(response))
  {
    const gchar *uri = webkit_uri_scheme_request_get_uri(request);
    if (!finish_scheme_from_native_route(instance, request, uri))
    {
      g_autoptr(GError) finish_error = g_error_new_literal(
          G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Resource not found");
      webkit_uri_scheme_request_finish_error(request, finish_error);
    }
    custom_scheme_request_context_free(context);
    return;
  }

  FlValue *value = fl_method_success_response_get_result(
      FL_METHOD_SUCCESS_RESPONSE(response));
  if (!value || fl_value_get_type(value) != FL_VALUE_TYPE_MAP)
  {
    const gchar *uri = webkit_uri_scheme_request_get_uri(request);
    if (!finish_scheme_from_native_route(instance, request, uri))
    {
      g_autoptr(GError) finish_error = g_error_new_literal(
          G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Resource not found");
      webkit_uri_scheme_request_finish_error(request, finish_error);
    }
    custom_scheme_request_context_free(context);
    return;
  }

  FlValue *data_value = fl_value_lookup_string(value, "data");
  FlValue *content_type_value = fl_value_lookup_string(value, "contentType");
  if (!data_value || fl_value_get_type(data_value) != FL_VALUE_TYPE_UINT8_LIST ||
      fl_value_get_length(data_value) == 0)
  {
    // Empty/miss from Dart (e.g. LocalhostServerService miss) — try native
    // scheme_routes before failing the WebKit request.
    const gchar *uri = webkit_uri_scheme_request_get_uri(request);
    if (!finish_scheme_from_native_route(instance, request, uri))
    {
      g_autoptr(GError) finish_error = g_error_new_literal(
          G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Resource not found");
      webkit_uri_scheme_request_finish_error(request, finish_error);
    }
    custom_scheme_request_context_free(context);
    return;
  }

  const uint8_t *bytes = fl_value_get_uint8_list(data_value);
  size_t length = fl_value_get_length(data_value);
  const gchar *content_type = "application/octet-stream";
  if (content_type_value &&
      fl_value_get_type(content_type_value) == FL_VALUE_TYPE_STRING)
  {
    content_type = fl_value_get_string(content_type_value);
  }

  gpointer copied = g_memdup2(bytes, length);
  GInputStream *stream =
      g_memory_input_stream_new_from_data(copied, length, g_free);
  webkit_uri_scheme_request_finish(request, stream, (gint64)length, content_type);
  g_object_unref(stream);
  custom_scheme_request_context_free(context);
}

// Create a new WebKitGTK WebView instance
WebViewWebKitGTK *webview_webkitgtk_new(
    FlMethodChannel *method_channel,
    gint64 view_id,
    FlValue *settings_map_or_null,
    WebKitWebContext *shared_context_or_null)
{
  WebViewWebKitGTK *instance = g_new0(WebViewWebKitGTK, 1);
  instance->method_channel = method_channel;
  instance->view_id = view_id;
  instance->zoom_level_after_load = 1.0;
  instance->flutter_block_network_loads = FALSE;
  instance->flutter_allow_file_access = FALSE;
  instance->flutter_geolocation_enabled = FALSE;

  instance->web_context =
      webview_webkitgtk_create_context_from_flutter_settings(
          settings_map_or_null, shared_context_or_null);

  // Create UserContentManager for JavaScript message handling
  instance->user_content_manager = webkit_user_content_manager_new();

  // Create hash table for scheme routes (path -> content)
  instance->scheme_routes = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, scheme_content_free);

  instance->script_handlers = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, script_handler_user_data_free);

  // Attach user_content_manager and web_context to the WebView via construct
  // properties. WebKitGTK 4.1 removed the standalone setter functions;
  // context and user-content-manager must be supplied at construction time.
  instance->web_view = WEBKIT_WEB_VIEW(
      g_object_new(WEBKIT_TYPE_WEB_VIEW,
                   "web-context", instance->web_context,
                   "user-content-manager", instance->user_content_manager,
                   nullptr));

  webkit_web_view_set_zoom_level(instance->web_view, instance->zoom_level_after_load);

  webview_webkitgtk_apply_flutter_settings_map(instance, settings_map_or_null);
  webview_webkitgtk_flutter_settings_install_handlers(instance);

  webview_webkitgtk_inject_bridge_script(instance);

  // Connect signals
  g_signal_connect(instance->web_view, "load-changed",
                   G_CALLBACK(on_load_changed), instance);
  g_signal_connect(instance->web_view, "load-failed",
                   G_CALLBACK(on_load_failed), instance);

  // InAppWebView-compatible JS channels (see flutter_inappwebview bridge script).
  webview_webkitgtk_register_message_handler(instance, "emailComposer");
  webview_webkitgtk_register_message_handler(instance, "openExternalUrl");

  register_webkit_instance(instance);

  // Always register appmsg://; also honor resourceCustomSchemes from Flutter.
  webview_webkitgtk_register_custom_schemes_from_settings(
      instance, settings_map_or_null);

  g_print("🐧 WebKitGTK WebView created (view_id: %ld)\n", view_id);

  return instance;
}

// Destroy WebKitGTK WebView instance
void webview_webkitgtk_destroy(WebViewWebKitGTK *instance)
{
  if (!instance)
    return;

  g_print("🐧 Destroying WebKitGTK WebView (view_id: %ld)\n", instance->view_id);

  unregister_webkit_instance(instance);

  // Tear down in an order WebKitGTK expects: the WebView must be removed and
  // destroyed while its WebContext is still valid. Unref'ing web_context while
  // the widget is still packed in a GtkWindow caused GObject criticals and
  // crashes when the window was destroyed next (e.g. mailbox dispose after UI
  // layout changes like opening the AI sidebar).

  if (instance->user_content_manager)
  {
    g_signal_handlers_disconnect_matched(instance->user_content_manager,
                                         G_SIGNAL_MATCH_DATA, 0, 0, nullptr,
                                         nullptr, instance);
  }

  if (instance->web_view)
  {
    GtkWidget *w = GTK_WIDGET(instance->web_view);
    g_signal_handlers_disconnect_matched(w, G_SIGNAL_MATCH_DATA, 0, 0, nullptr,
                                         nullptr, instance);

    if (gtk_widget_get_parent(w))
    {
      GtkWidget *parent = gtk_widget_get_parent(w);
      g_object_ref(w);
      gtk_container_remove(GTK_CONTAINER(parent), w);
      gtk_widget_destroy(w);
      g_object_unref(w);
    }
    else
    {
      gtk_widget_destroy(w);
    }
    instance->web_view = nullptr;
  }

  if (instance->web_context)
  {
    g_object_unref(instance->web_context);
    instance->web_context = nullptr;
  }

  if (instance->user_content_manager)
  {
    g_object_unref(instance->user_content_manager);
    instance->user_content_manager = nullptr;
  }

  if (instance->scheme_routes)
  {
    g_hash_table_destroy(instance->scheme_routes);
    instance->scheme_routes = nullptr;
  }

  if (instance->script_handlers)
  {
    g_hash_table_destroy(instance->script_handlers);
    instance->script_handlers = nullptr;
  }

  g_free(instance);
}

// Get the GTK widget (for platform view embedding)
GtkWidget *webview_webkitgtk_get_widget(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view)
  {
    return nullptr;
  }
  return GTK_WIDGET(instance->web_view);
}

// Load URL
void webview_webkitgtk_load_url(WebViewWebKitGTK *instance,
                                const gchar *url,
                                FlValue *headers_map_or_null)
{
  if (!instance || !instance->web_view || !url)
  {
    g_warning("🐧 Cannot load URL: invalid instance or URL");
    return;
  }

  g_print("🐧 Loading URL: %s\n", url);
  WebKitURIRequest *request = webkit_uri_request_new(url);
  if (headers_map_or_null &&
      fl_value_get_type(headers_map_or_null) == FL_VALUE_TYPE_MAP)
  {
    SoupMessageHeaders *headers = webkit_uri_request_get_http_headers(request);
    const size_t count = fl_value_get_length(headers_map_or_null);
    for (size_t i = 0; i < count; i++)
    {
      FlValue *name = fl_value_get_map_key(headers_map_or_null, i);
      FlValue *value = fl_value_get_map_value(headers_map_or_null, i);
      if (name && value &&
          fl_value_get_type(name) == FL_VALUE_TYPE_STRING &&
          fl_value_get_type(value) == FL_VALUE_TYPE_STRING)
      {
        soup_message_headers_replace(headers, fl_value_get_string(name),
                                     fl_value_get_string(value));
      }
    }
  }
  webkit_web_view_load_request(instance->web_view, request);
  g_object_unref(request);
}

// Load HTML
void webview_webkitgtk_load_html(WebViewWebKitGTK *instance,
                                 const gchar *html,
                                 const gchar *base_url)
{
  if (!instance || !instance->web_view || !html)
  {
    g_warning("🐧 Cannot load HTML: invalid instance or HTML");
    return;
  }

  g_print("🐧 Loading HTML (base_url: %s)\n", base_url ? base_url : "null");
  webkit_web_view_load_html(instance->web_view, html, base_url);
}

// Evaluate JavaScript
void webview_webkitgtk_evaluate_javascript(
    WebViewWebKitGTK *instance,
    const gchar *javascript,
    FlMethodCall *method_call)
{
  if (!instance || !instance->web_view || !javascript)
  {
    g_warning("🐧 Cannot evaluate JavaScript: invalid instance or script");
    if (method_call)
    {
      g_autoptr(FlMethodResponse) response =
          FL_METHOD_RESPONSE(fl_method_error_response_new(
              "INVALID_ARGUMENT", "Invalid instance or JavaScript", nullptr));
      fl_method_call_respond(method_call, response, nullptr);
    }
    return;
  }

  g_print("🐧 Evaluating JavaScript: %s\n", javascript);

  // Use webkit_web_view_evaluate_javascript to evaluate JavaScript (newer API)
  webkit_web_view_evaluate_javascript(
      instance->web_view,
      javascript,
      -1,      // length (-1 = null-terminated)
      nullptr, // world_name (use default)
      nullptr, // source_uri
      nullptr, // cancellable
      [](GObject *source_object, GAsyncResult *res, gpointer user_data)
      {
        FlMethodCall *call = FL_METHOD_CALL(user_data);
        g_autoptr(FlMethodResponse) response = nullptr;

        GError *error = nullptr;
        // In WebKitGTK 4.1+, evaluate_javascript_finish returns JSCValue* directly
        JSCValue *value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source_object),
                                                                     res, &error);

        if (error)
        {
          if (g_strcmp0(error->message, "Unsupported result type") != 0)
          {
            g_warning("🐧 JavaScript evaluation error: %s", error->message);
          }
          response = FL_METHOD_RESPONSE(fl_method_error_response_new(
              "JAVASCRIPT_ERROR", error->message, nullptr));
          g_error_free(error);
        }
        else if (value)
        {
          // Extract result from JavaScript
          gchar *result_str = nullptr;

          if (jsc_value_is_string(value))
          {
            result_str = jsc_value_to_string(value);
          }
          else
          {
            result_str = jsc_value_to_json(value, 0);
            // jsc_value_to_json returns NULL for non-JSON-serializable values
            // (e.g. undefined, functions). Guard against NULL before passing to
            // fl_value_new_string which requires a non-NULL C string.
            if (result_str == nullptr)
            {
              result_str = g_strdup("null");
            }
          }

          g_autoptr(FlValue) result_value = fl_value_new_string(result_str);
          response = FL_METHOD_RESPONSE(fl_method_success_response_new(result_value));
          g_free(result_str);
          g_object_unref(value);
        }
        else
        {
          response = FL_METHOD_RESPONSE(fl_method_error_response_new(
              "JAVASCRIPT_ERROR", "No result returned", nullptr));
        }

        fl_method_call_respond(call, response, nullptr);
        g_object_unref(call);
      },
      g_object_ref(method_call));
}

// Reload
void webview_webkitgtk_reload(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view)
  {
    return;
  }
  g_print("🐧 Reloading WebView\n");
  webkit_web_view_reload(instance->web_view);
}

// Go back
void webview_webkitgtk_go_back(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view)
  {
    return;
  }
  if (webkit_web_view_can_go_back(instance->web_view))
  {
    g_print("🐧 Going back\n");
    webkit_web_view_go_back(instance->web_view);
  }
}

// Check if can go back
gboolean webview_webkitgtk_can_go_back(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view)
  {
    return FALSE;
  }
  return webkit_web_view_can_go_back(instance->web_view);
}

// Get current URL
gchar *webview_webkitgtk_get_current_url(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view)
  {
    return nullptr;
  }
  const gchar *uri = webkit_web_view_get_uri(instance->web_view);
  return uri ? g_strdup(uri) : nullptr;
}

// Scheme request callback — prefers Dart onLoadResourceWithCustomScheme,
// then falls back to the legacy native scheme_routes table.
static void custom_scheme_request_callback(WebKitURISchemeRequest *request,
                                           gpointer user_data)
{
  WebViewWebKitGTK *instance =
      instance_for_scheme_request(request, user_data);

  if (!instance || !request)
  {
    g_warning("🐧 Scheme request callback: Invalid instance");
    webkit_uri_scheme_request_finish_error(
        request, g_error_new_literal(G_IO_ERROR, G_IO_ERROR_FAILED,
                                     "Internal error"));
    return;
  }

  const gchar *uri = webkit_uri_scheme_request_get_uri(request);
  g_print("🐧 Scheme request viewId=%ld: %s\n",
          (long)instance->view_id, uri ? uri : "(null)");

  if (!instance->method_channel)
  {
    if (!finish_scheme_from_native_route(instance, request, uri))
    {
      webkit_uri_scheme_request_finish_error(
          request, g_error_new_literal(G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                       "Route not found"));
    }
    return;
  }

  g_autoptr(FlValue) args = fl_value_new_map();
  fl_value_set_string_take(args, "viewId",
                           fl_value_new_int((int64_t)instance->view_id));

  g_autoptr(FlValue) request_map = fl_value_new_map();
  fl_value_set_string_take(request_map, "url",
                           fl_value_new_string(uri ? uri : ""));
  const gchar *http_method =
      webkit_uri_scheme_request_get_http_method(request);
  fl_value_set_string_take(
      request_map, "method",
      fl_value_new_string(http_method ? http_method : "GET"));
  fl_value_set_string_take(request_map, "isForMainFrame",
                           fl_value_new_bool(TRUE));

  g_autoptr(FlValue) headers_map = fl_value_new_map();
  SoupMessageHeaders *headers =
      webkit_uri_scheme_request_get_http_headers(request);
  if (headers != nullptr)
  {
    SoupMessageHeadersIter iter;
    const char *name = nullptr;
    const char *value = nullptr;
    soup_message_headers_iter_init(&iter, headers);
    while (soup_message_headers_iter_next(&iter, &name, &value))
    {
      if (name != nullptr && value != nullptr)
      {
        fl_value_set_string_take(headers_map, name,
                                 fl_value_new_string(value));
      }
    }
  }
  fl_value_set_string_take(request_map, "headers",
                           fl_value_ref(headers_map));
  fl_value_set_string_take(args, "request", fl_value_ref(request_map));

  CustomSchemeRequestContext *context = g_new0(CustomSchemeRequestContext, 1);
  context->request = WEBKIT_URI_SCHEME_REQUEST(g_object_ref(request));
  context->instance = instance;

  fl_method_channel_invoke_method(
      instance->method_channel, "onLoadResourceWithCustomScheme", args,
      nullptr, on_dart_custom_scheme_result, context);
}

// Register custom scheme handler (forwards to Dart + native fallback).
void webview_webkitgtk_register_custom_scheme(
    WebViewWebKitGTK *instance,
    const gchar *scheme)
{
  if (!instance || !instance->web_context || !scheme)
  {
    return;
  }

  // Built-in schemes cannot be overridden.
  if (g_strcmp0(scheme, "http") == 0 || g_strcmp0(scheme, "https") == 0 ||
      g_strcmp0(scheme, "file") == 0 || g_strcmp0(scheme, "data") == 0 ||
      g_strcmp0(scheme, "about") == 0 || g_strcmp0(scheme, "blob") == 0)
  {
    g_warning("🐧 Refusing to register built-in scheme: %s", scheme);
    return;
  }

  // Shared contexts may only register a given scheme once. Subsequent views
  // rely on instance_for_scheme_request() to resolve the correct wrapper.
  g_autofree gchar *marker_key =
      g_strdup_printf("scomm-uri-scheme-registered-%s", scheme);
  if (g_object_get_data(G_OBJECT(instance->web_context), marker_key) != nullptr)
  {
    g_print("🐧 Custom scheme '%s' already registered on shared context "
            "(view_id: %ld)\n",
            scheme, (long)instance->view_id);
    return;
  }

  g_print("🐧 Registering custom scheme: %s\n", scheme);

  webkit_web_context_register_uri_scheme(
      instance->web_context,
      scheme,
      custom_scheme_request_callback,
      instance,
      nullptr);

  g_object_set_data(G_OBJECT(instance->web_context), marker_key,
                    GINT_TO_POINTER(1));

  g_print("🐧 Custom scheme '%s' registered successfully\n", scheme);
}

void webview_webkitgtk_register_custom_schemes_from_settings(
    WebViewWebKitGTK *instance,
    FlValue *settings_map_or_null)
{
  if (!instance)
  {
    return;
  }

  // Always register appmsg for secMail / legacy callers.
  webview_webkitgtk_register_custom_scheme(instance, "appmsg");

  if (!settings_map_or_null ||
      fl_value_get_type(settings_map_or_null) != FL_VALUE_TYPE_MAP)
  {
    return;
  }

  FlValue *schemes_value =
      fl_value_lookup_string(settings_map_or_null, "resourceCustomSchemes");
  if (!schemes_value || fl_value_get_type(schemes_value) != FL_VALUE_TYPE_LIST)
  {
    return;
  }

  const size_t count = fl_value_get_length(schemes_value);
  for (size_t i = 0; i < count; i++)
  {
    FlValue *item = fl_value_get_list_value(schemes_value, i);
    if (!item || fl_value_get_type(item) != FL_VALUE_TYPE_STRING)
    {
      continue;
    }
    const gchar *scheme = fl_value_get_string(item);
    if (!scheme || g_strcmp0(scheme, "appmsg") == 0)
    {
      continue;
    }
    webview_webkitgtk_register_custom_scheme(instance, scheme);
  }
}

// Register a route for the custom scheme
void webview_webkitgtk_register_scheme_route(
    WebViewWebKitGTK *instance,
    const gchar *path,
    const gchar *content,
    gsize content_length,
    const gchar *content_type,
    gboolean is_text)
{
  if (!instance || !instance->scheme_routes || !path || !content)
  {
    g_warning("🐧 Cannot register scheme route: invalid arguments");
    return;
  }

  // Normalize path (remove leading slash if present, for consistency)
  const gchar *normalized_path = path;
  if (*path == '/')
  {
    normalized_path = path + 1;
  }

  // Create content structure
  WebViewSchemeContent *scheme_content = g_new0(WebViewSchemeContent, 1);
  scheme_content->content = (gchar *)g_memdup2(content, content_length);
  scheme_content->content_length = content_length;
  scheme_content->content_type = g_strdup(content_type ? content_type : "application/octet-stream");
  scheme_content->is_text = is_text;

  // Insert or replace in hash table
  g_hash_table_insert(instance->scheme_routes,
                      g_strdup(normalized_path),
                      scheme_content);

  g_print("🐧 Registered scheme route: %s (%zu bytes, %s)\n",
          normalized_path, content_length, scheme_content->content_type);
}

// Unregister a scheme route
void webview_webkitgtk_unregister_scheme_route(
    WebViewWebKitGTK *instance,
    const gchar *path)
{
  if (!instance || !instance->scheme_routes || !path)
  {
    return;
  }

  // Normalize path
  const gchar *normalized_path = path;
  if (*path == '/')
  {
    normalized_path = path + 1;
  }

  gboolean removed = g_hash_table_remove(instance->scheme_routes, normalized_path);
  if (removed)
  {
    g_print("🐧 Unregistered scheme route: %s\n", normalized_path);
  }
  else
  {
    g_print("🐧 Scheme route not found for removal: %s\n", normalized_path);
  }
}

// Register JavaScript message handler and connect script-message-received signal.
void webview_webkitgtk_register_message_handler(
    WebViewWebKitGTK *instance,
    const gchar *handler_name)
{
  if (!instance || !instance->user_content_manager || !handler_name ||
      !instance->script_handlers)
  {
    return;
  }

  if (g_hash_table_contains(instance->script_handlers, handler_name))
  {
    return;
  }

  g_print("🐧 Registering message handler: %s\n", handler_name);

  webkit_user_content_manager_register_script_message_handler(
      instance->user_content_manager, handler_name);

  ScriptHandlerUserData *handler_data = g_new0(ScriptHandlerUserData, 1);
  handler_data->instance = instance;
  handler_data->handler_name = g_strdup(handler_name);

  gchar *signal_name =
      g_strdup_printf("script-message-received::%s", handler_name);
  g_signal_connect(
      instance->user_content_manager, signal_name,
      G_CALLBACK(on_script_message_for_handler), handler_data);
  g_free(signal_name);

  g_hash_table_insert(
      instance->script_handlers, g_strdup(handler_name), handler_data);
}

// Set zoom level (1.0 = 100%, 1.5 = 150%, etc.)
void webview_webkitgtk_set_zoom_level(
    WebViewWebKitGTK *instance,
    double zoom_level)
{
  if (!instance || !instance->web_view)
  {
    return;
  }

  g_print("🐧 Setting zoom level to: %.2f\n", zoom_level);
  instance->zoom_level_after_load = zoom_level;
  webkit_web_view_set_zoom_level(instance->web_view, zoom_level);
}

// Signal handler: load-changed
static void on_load_changed(WebKitWebView *web_view,
                            WebKitLoadEvent load_event,
                            gpointer user_data)
{
  WebViewWebKitGTK *instance =
      static_cast<WebViewWebKitGTK *>(user_data);

  if (!instance || !instance->method_channel)
  {
    return;
  }

  switch (load_event)
  {
  case WEBKIT_LOAD_STARTED:
  {
    const gchar *uri = webkit_web_view_get_uri(web_view);
    const gchar *url = uri ? uri : "";
    g_autoptr(FlValue) args = fl_value_new_map();
    fl_value_set_string_take(args, "url", fl_value_new_string(url));
    fl_method_channel_invoke_method(
        instance->method_channel, "onLoadStart", args, nullptr, nullptr, nullptr);
    break;
  }
  case WEBKIT_LOAD_COMMITTED:
    // Page committed to load
    break;
  case WEBKIT_LOAD_FINISHED:
  {
    const gchar *uri = webkit_web_view_get_uri(web_view);
    const gchar *url = uri ? uri : "";
    webkit_web_view_set_zoom_level(web_view, instance->zoom_level_after_load);
    g_autoptr(FlValue) args2 = fl_value_new_map();
    fl_value_set_string_take(args2, "url", fl_value_new_string(url));
    fl_method_channel_invoke_method(
        instance->method_channel, "onLoadStop", args2, nullptr, nullptr, nullptr);
    break;
  }
  default:
    break;
  }
}

// Signal handler: load-failed
static void on_load_failed(WebKitWebView *web_view,
                           WebKitLoadEvent load_event,
                           const gchar *failing_uri,
                           GError *error,
                           gpointer user_data)
{
  WebViewWebKitGTK *instance =
      static_cast<WebViewWebKitGTK *>(user_data);

  if (!instance)
  {
    return;
  }

  gint error_code = error ? error->code : -1;
  const gchar *error_message = error ? error->message : "Unknown error";

  if (error_code != 102 && error_code != 302)
  {
    g_warning("🐧 Load failed: %s (code: %d, message: %s)\n",
              failing_uri ? failing_uri : "null", error_code, error_message);
  }

  if (!instance->method_channel)
  {
    return;
  }

  const gchar *url = failing_uri ? failing_uri : "";
  g_autoptr(FlValue) args = fl_value_new_map();
  fl_value_set_string_take(args, "url", fl_value_new_string(url));
  fl_value_set_string_take(args, "code", fl_value_new_int(error_code));
  fl_value_set_string_take(
      args, "message", fl_value_new_string(error_message));
  fl_method_channel_invoke_method(
      instance->method_channel, "onLoadError", args, nullptr, nullptr, nullptr);
}

// Signal handler: script-message-received::<handler_name>
static void on_script_message_for_handler(WebKitUserContentManager *manager,
                                            WebKitJavascriptResult *js_result,
                                            gpointer user_data)
{
  ScriptHandlerUserData *handler_data =
      static_cast<ScriptHandlerUserData *>(user_data);

  if (!handler_data || !handler_data->instance || !handler_data->handler_name)
  {
    return;
  }

  // webkit_javascript_result_get_js_value returns transfer-none (borrowed ref)
  // Do NOT use g_autoptr here as we don't own this JSCValue
  JSCValue *value = webkit_javascript_result_get_js_value(js_result);
  g_autofree gchar *payload = nullptr;

  if (value)
  {
    if (jsc_value_is_string(value))
    {
      payload = jsc_value_to_string(value);
    }
    else
    {
      payload = jsc_value_to_json(value, 0);
    }
    g_print("🐧 JavaScript message [%s]: %s\n",
            handler_data->handler_name, payload);
  }

  webview_webkitgtk_send_on_message(
      handler_data->instance, handler_data->handler_name, payload);
}

static WebKitUserScriptInjectionTime injection_time_from_string(
    const gchar *value)
{
  if (value && g_strcmp0(value, "AT_DOCUMENT_END") == 0)
  {
    return WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
  }
  return WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;
}

void webview_webkitgtk_add_user_scripts_from_flvalue(
    WebViewWebKitGTK *instance,
    FlValue *scripts_list_or_null)
{
  if (!instance || !instance->user_content_manager || !scripts_list_or_null)
  {
    return;
  }
  if (fl_value_get_type(scripts_list_or_null) != FL_VALUE_TYPE_LIST)
  {
    return;
  }

  const size_t count = fl_value_get_length(scripts_list_or_null);
  for (size_t i = 0; i < count; i++)
  {
    FlValue *entry = fl_value_get_list_value(scripts_list_or_null, i);
    if (!entry || fl_value_get_type(entry) != FL_VALUE_TYPE_MAP)
    {
      continue;
    }
    FlValue *source_value = fl_value_lookup_string(entry, "source");
    if (!source_value || fl_value_get_type(source_value) != FL_VALUE_TYPE_STRING)
    {
      continue;
    }
    const gchar *source = fl_value_get_string(source_value);
    WebKitUserScriptInjectionTime injection_time =
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;
    FlValue *time_value = fl_value_lookup_string(entry, "injectionTime");
    if (time_value && fl_value_get_type(time_value) == FL_VALUE_TYPE_STRING)
    {
      injection_time = injection_time_from_string(fl_value_get_string(time_value));
    }
    gboolean for_main_frame_only = TRUE;
    FlValue *main_frame_value = fl_value_lookup_string(entry, "forMainFrameOnly");
    if (main_frame_value && fl_value_get_type(main_frame_value) == FL_VALUE_TYPE_BOOL)
    {
      for_main_frame_only = fl_value_get_bool(main_frame_value);
    }

    WebKitUserScript *script = webkit_user_script_new(
        source,
        for_main_frame_only ? WEBKIT_USER_CONTENT_INJECT_TOP_FRAME
                            : WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        injection_time,
        nullptr,
        nullptr);
    webkit_user_content_manager_add_script(instance->user_content_manager, script);
    webkit_user_script_unref(script);
  }
}
