#include "webview_plugin_content_handlers.h"

#include <flutter_linux/flutter_linux.h>
#include <glib.h>
#include <string.h>

#include "webview_overlay_window.h"
#include "webview_webkitgtk.h"
#include "webview_plugin_methods.h"

bool webview_plugin_try_handle_content_method(
    GHashTable *overlay_windows,
    const gchar *method,
    FlValue *args,
    FlMethodCall *method_call,
    FlMethodResponse **out_response)
{
  *out_response = nullptr;

  if (g_strcmp0(method, kMethodLoadUrl) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");
    FlValue *url_value = fl_value_lookup_string(args, "url");
    FlValue *headers_value = fl_value_lookup_string(args, "headers");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));
      return true;
    }

    if (url_value == nullptr || fl_value_get_type(url_value) != FL_VALUE_TYPE_STRING)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "url must be a string", nullptr));
      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    const gchar *url = fl_value_get_string(url_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);

    WebViewOverlayWindow *overlay_window =
        (WebViewOverlayWindow *)g_hash_table_lookup(overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Overlay window not found", nullptr));
      return true;
    }

    webview_webkitgtk_load_url(overlay_window->webkit_view, url, headers_value);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodLoadHtml) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");
    FlValue *html_value = fl_value_lookup_string(args, "html");
    FlValue *base_url_value = fl_value_lookup_string(args, "baseUrl");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT ||
        html_value == nullptr || fl_value_get_type(html_value) != FL_VALUE_TYPE_STRING)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId/html are invalid", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    const gchar *html = fl_value_get_string(html_value);
    const gchar *base_url = base_url_value ? fl_value_get_string(base_url_value) : nullptr;
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);

    WebViewOverlayWindow *overlay_window =
        (WebViewOverlayWindow *)g_hash_table_lookup(overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Platform view not found", nullptr));

      return true;
    }

    webview_webkitgtk_load_html(overlay_window->webkit_view, html, base_url);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodEvaluateJavaScript) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");
    FlValue *javascript_value = fl_value_lookup_string(args, "javascript");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT ||
        javascript_value == nullptr ||
        fl_value_get_type(javascript_value) != FL_VALUE_TYPE_STRING)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId/javascript are invalid", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    const gchar *javascript = fl_value_get_string(javascript_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);

    WebViewOverlayWindow *overlay_window =
        (WebViewOverlayWindow *)g_hash_table_lookup(overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Platform view not found", nullptr));

      return true;
    }

    webview_webkitgtk_evaluate_javascript(
        overlay_window->webkit_view, javascript, method_call);

    return true; // async response handled in callback
  }

  if (g_strcmp0(method, kMethodReload) == 0 || g_strcmp0(method, kMethodGoBack) == 0 ||
      g_strcmp0(method, kMethodCanGoBack) == 0 || g_strcmp0(method, kMethodGetCurrentUrl) == 0 ||
      g_strcmp0(method, kMethodSetZoom) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId must be an integer", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);

    WebViewOverlayWindow *overlay_window =
        (WebViewOverlayWindow *)g_hash_table_lookup(overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Platform view not found", nullptr));

      return true;
    }

    WebViewWebKitGTK *webkit_view = overlay_window->webkit_view;

    if (g_strcmp0(method, kMethodReload) == 0)
    {
      webview_webkitgtk_reload(webkit_view);
      *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

      return true;
    }

    if (g_strcmp0(method, kMethodGoBack) == 0)
    {
      webview_webkitgtk_go_back(webkit_view);
      *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

      return true;
    }

    if (g_strcmp0(method, kMethodCanGoBack) == 0)
    {
      gboolean can_go_back = webview_webkitgtk_can_go_back(webkit_view);
      g_autoptr(FlValue) result_value = fl_value_new_bool(can_go_back);
      *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(result_value));

      return true;
    }

    if (g_strcmp0(method, kMethodGetCurrentUrl) == 0)
    {
      gchar *url = webview_webkitgtk_get_current_url(webkit_view);

      if (url)
      {
        g_autoptr(FlValue) result_value = fl_value_new_string(url);
        *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(result_value));
        g_free(url);
      }
      else
      {
        *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
      }

      return true;
    }

    FlValue *zoom_value = fl_value_lookup_string(args, "zoom");

    if (zoom_value == nullptr)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "zoom must be provided", nullptr));

      return true;
    }

    double zoom = 1.0;

    if (fl_value_get_type(zoom_value) == FL_VALUE_TYPE_FLOAT)
    {
      zoom = fl_value_get_float(zoom_value);
    }
    else if (fl_value_get_type(zoom_value) == FL_VALUE_TYPE_INT)
    {
      zoom = (double)fl_value_get_int(zoom_value);
    }
    else
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "zoom must be a number", nullptr));
      return true;
    }

    webview_webkitgtk_set_zoom_level(webkit_view, zoom);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodAddJavaScriptChannel) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");
    FlValue *channel_name_value = fl_value_lookup_string(args, "channelName");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT ||
        channel_name_value == nullptr ||
        fl_value_get_type(channel_name_value) != FL_VALUE_TYPE_STRING)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId/channelName are invalid", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    const gchar *channel_name = fl_value_get_string(channel_name_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);

    WebViewOverlayWindow *overlay_window =
        (WebViewOverlayWindow *)g_hash_table_lookup(overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Platform view not found", nullptr));

      return true;
    }

    webview_webkitgtk_register_message_handler(overlay_window->webkit_view, channel_name);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  if (g_strcmp0(method, kMethodRegisterSchemeRoute) == 0 ||
      g_strcmp0(method, kMethodUnregisterSchemeRoute) == 0)
  {
    FlValue *view_id_value = fl_value_lookup_string(args, "viewId");
    FlValue *path_value = fl_value_lookup_string(args, "path");

    if (view_id_value == nullptr || fl_value_get_type(view_id_value) != FL_VALUE_TYPE_INT ||
        path_value == nullptr || fl_value_get_type(path_value) != FL_VALUE_TYPE_STRING)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "viewId/path are invalid", nullptr));

      return true;
    }

    gint64 view_id = fl_value_get_int(view_id_value);
    const gchar *path = fl_value_get_string(path_value);
    gpointer view_id_key = GSIZE_TO_POINTER((gsize)view_id);

    WebViewOverlayWindow *overlay_window =
        (WebViewOverlayWindow *)g_hash_table_lookup(overlay_windows, view_id_key);

    if (!overlay_window)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "NOT_FOUND", "Platform view not found", nullptr));

      return true;
    }

    if (g_strcmp0(method, kMethodUnregisterSchemeRoute) == 0)
    {
      webview_webkitgtk_unregister_scheme_route(overlay_window->webkit_view, path);
      *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

      return true;
    }

    FlValue *content_value = fl_value_lookup_string(args, "content");
    FlValue *content_type_value = fl_value_lookup_string(args, "contentType");
    FlValue *is_text_value = fl_value_lookup_string(args, "isText");

    if (content_value == nullptr)
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "content must be provided", nullptr));

      return true;
    }

    const gchar *content_type = content_type_value ? fl_value_get_string(content_type_value) : nullptr;

    gboolean is_text = TRUE;
    FlValueType content_type_enum = fl_value_get_type(content_value);
    const gchar *content_bytes = nullptr;
    gsize content_length = 0;

    if (content_type_enum == FL_VALUE_TYPE_STRING)
    {
      content_bytes = fl_value_get_string(content_value);

      content_length = strlen(content_bytes);
      is_text = TRUE;
    }
    else if (content_type_enum == FL_VALUE_TYPE_UINT8_LIST)
    {
      const uint8_t *bytes = fl_value_get_uint8_list(content_value);
      content_length = fl_value_get_length(content_value);

      content_bytes = (const gchar *)bytes;
      is_text = FALSE;
    }
    else
    {
      *out_response = FL_METHOD_RESPONSE(fl_method_error_response_new(
          "INVALID_ARGUMENT", "content must be a string or Uint8List", nullptr));

      return true;
    }

    if (is_text_value && fl_value_get_type(is_text_value) == FL_VALUE_TYPE_BOOL)
    {
      is_text = fl_value_get_bool(is_text_value);
    }

    if (content_type == nullptr)
    {
      content_type = is_text ? "text/html; charset=utf-8" : "application/octet-stream";
    }

    webview_webkitgtk_register_scheme_route(
        overlay_window->webkit_view, path, content_bytes, content_length, content_type, is_text);
    *out_response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));

    return true;
  }

  return false;
}
