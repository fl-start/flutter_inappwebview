// Generic mapping from optional Flutter "settings" maps to WebKitGTK.
// Kept free of app/package identifiers so any embedder can reuse the contract.

#include "webview_webkitgtk_flutter_settings.h"

#include "webview_webkitgtk.h"

#include <glib.h>

namespace
{

  gboolean flutter_map_get_bool(FlValue *map, const gchar *key, gboolean def_val)
  {
    if (!map || fl_value_get_type(map) != FL_VALUE_TYPE_MAP)
    {
      return def_val;
    }
    FlValue *v = fl_value_lookup_string(map, key);
    if (!v)
    {
      return def_val;
    }
    if (fl_value_get_type(v) == FL_VALUE_TYPE_BOOL)
    {
      return fl_value_get_bool(v);
    }
    if (fl_value_get_type(v) == FL_VALUE_TYPE_INT)
    {
      return fl_value_get_int(v) != 0;
    }
    return def_val;
  }

  gint flutter_map_get_int(FlValue *map, const gchar *key, gint def_val)
  {
    if (!map || fl_value_get_type(map) != FL_VALUE_TYPE_MAP)
    {
      return def_val;
    }
    FlValue *v = fl_value_lookup_string(map, key);
    if (!v)
    {
      return def_val;
    }
    if (fl_value_get_type(v) == FL_VALUE_TYPE_INT)
    {
      return static_cast<gint>(fl_value_get_int(v));
    }
    if (fl_value_get_type(v) == FL_VALUE_TYPE_FLOAT)
    {
      return static_cast<gint>(fl_value_get_float(v) + 0.5);
    }
    return def_val;
  }

  void apply_context_level_from_map(WebKitWebContext *ctx, FlValue *map)
  {
    if (!ctx)
    {
      return;
    }

    const gboolean enable_cache = flutter_map_get_bool(map, "enableCache", TRUE);
    const WebKitCacheModel cache_model =
        enable_cache ? WEBKIT_CACHE_MODEL_WEB_BROWSER : WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER;
    webkit_web_context_set_cache_model(ctx, cache_model);

    const gboolean block_third_party = flutter_map_get_bool(map, "blockThirdPartyCookies", FALSE);
    const WebKitCookieAcceptPolicy cookie_policy =
        block_third_party ? WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY
                          : WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;
    WebKitCookieManager *cm = webkit_web_context_get_cookie_manager(ctx);
    if (cm)
    {
      webkit_cookie_manager_set_accept_policy(cm, cookie_policy);
    }
  }

  gboolean uri_is_http_loopback(const gchar *uri)
  {
    if (!uri)
    {
      return FALSE;
    }
    g_autoptr(GError) error = nullptr;
    g_autoptr(GUri) parsed = g_uri_parse(uri, G_URI_FLAGS_PARSE_RELAXED, &error);
    if (!parsed)
      return FALSE;

    const gchar *scheme = g_uri_get_scheme(parsed);
    const gchar *host = g_uri_get_host(parsed);
    if (!scheme || !host ||
        (g_ascii_strcasecmp(scheme, "http") != 0 &&
         g_ascii_strcasecmp(scheme, "https") != 0))
      return FALSE;

    return g_ascii_strcasecmp(host, "localhost") == 0 ||
           g_strcmp0(host, "127.0.0.1") == 0 ||
           g_strcmp0(host, "::1") == 0;
  }

  gboolean uri_is_remote_http_or_https(const gchar *uri)
  {
    if (!uri)
    {
      return FALSE;
    }
    g_autoptr(GUri) parsed = g_uri_parse(uri, G_URI_FLAGS_PARSE_RELAXED, nullptr);
    if (!parsed)
      return FALSE;
    const gchar *scheme = g_uri_get_scheme(parsed);
    return scheme && (g_ascii_strcasecmp(scheme, "http") == 0 ||
                      g_ascii_strcasecmp(scheme, "https") == 0);
  }

  extern "C" gboolean webview_flutter_on_decide_policy(WebKitWebView *web_view,
                                                       WebKitPolicyDecision *decision,
                                                       WebKitPolicyDecisionType decision_type,
                                                       gpointer user_data)
  {
    WebViewWebKitGTK *inst = static_cast<WebViewWebKitGTK *>(user_data);
    (void)web_view;
    if (!inst || !decision)
    {
      return FALSE;
    }

    const gchar *uri = nullptr;

    if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    {
      WebKitNavigationPolicyDecision *nd = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
      WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nd);
      WebKitURIRequest *req = webkit_navigation_action_get_request(action);
      uri = webkit_uri_request_get_uri(req);
    }
    else if (decision_type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
    {
      WebKitResponsePolicyDecision *rd = WEBKIT_RESPONSE_POLICY_DECISION(decision);
      WebKitURIRequest *req = webkit_response_policy_decision_get_request(rd);
      uri = webkit_uri_request_get_uri(req);
    }
    else
    {
      return FALSE;
    }

    if (!uri)
    {
      return FALSE;
    }

    if (inst->flutter_block_network_loads && uri_is_remote_http_or_https(uri) &&
        !uri_is_http_loopback(uri))
    {
      webkit_policy_decision_ignore(decision);
      return TRUE;
    }

    if (!inst->flutter_allow_file_access && g_str_has_prefix(uri, "file:"))
    {
      webkit_policy_decision_ignore(decision);
      return TRUE;
    }

    return FALSE;
  }

  extern "C" void webview_flutter_on_permission_request(WebKitWebView *web_view,
                                                        WebKitPermissionRequest *request,
                                                        gpointer user_data)
  {
    WebViewWebKitGTK *inst = static_cast<WebViewWebKitGTK *>(user_data);
    (void)web_view;
    if (!inst || !request)
    {
      return;
    }

    // Geolocation (type name stable across WebKitGTK 4.0 / 4.1).
    if (g_strcmp0(g_type_name(G_OBJECT_TYPE(request)), "WebKitGeolocationPermissionRequest") ==
        0)
    {
      if (inst->flutter_geolocation_enabled)
      {
        webkit_permission_request_allow(request);
      }
      else
      {
        webkit_permission_request_deny(request);
      }
      return;
    }

    // Default-deny unknown permission types for a hardened generic viewer.
    webkit_permission_request_deny(request);
  }

} // namespace

WebKitWebContext *webview_webkitgtk_create_context_from_flutter_settings(FlValue *map)
{
  const gboolean incognito = flutter_map_get_bool(map, "incognito", FALSE);

  WebKitWebContext *ctx = nullptr;
  if (incognito)
  {
    WebKitWebsiteDataManager *dm = webkit_website_data_manager_new_ephemeral();
    ctx = webkit_web_context_new_with_website_data_manager(dm);
    g_object_unref(dm);
  }
  else
  {
    ctx = webkit_web_context_new();
  }

  apply_context_level_from_map(ctx, map);
  return ctx;
}

void webview_webkitgtk_apply_flutter_settings_map(WebViewWebKitGTK *instance, FlValue *map)
{
  if (!instance || !instance->web_view || !instance->web_context)
  {
    return;
  }
  if (!map || fl_value_get_type(map) != FL_VALUE_TYPE_MAP)
  {
    // Same defaults as missing keys in a map (typical embedder defaults).
    instance->flutter_block_network_loads = FALSE;
    instance->flutter_allow_file_access = FALSE;
    instance->flutter_geolocation_enabled = FALSE;
    apply_context_level_from_map(instance->web_context, nullptr);
    WebKitSettings *settings = webkit_web_view_get_settings(instance->web_view);
    if (!settings)
    {
      return;
    }
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_auto_load_images(settings, TRUE);
    webkit_settings_set_zoom_text_only(settings, TRUE);
    webkit_settings_set_media_playback_requires_user_gesture(settings, TRUE);
    webkit_settings_set_enable_html5_local_storage(settings, FALSE);
    webkit_settings_set_minimum_font_size(settings, 8);
    webkit_settings_set_default_font_size(settings, 16);
    webkit_settings_set_default_monospace_font_size(settings, 14);
    webkit_settings_set_enable_smooth_scrolling(settings, TRUE);
    webkit_settings_set_enable_media_stream(settings, FALSE);
    webkit_settings_set_enable_webaudio(settings, FALSE);
    webkit_settings_set_enable_webgl(settings, FALSE);
    return;
  }

  apply_context_level_from_map(instance->web_context, map);

  instance->flutter_block_network_loads = flutter_map_get_bool(map, "blockNetworkLoads", FALSE);
  instance->flutter_allow_file_access = flutter_map_get_bool(map, "allowFileAccess", FALSE);
  instance->flutter_geolocation_enabled = flutter_map_get_bool(map, "geolocationEnabled", FALSE);

  WebKitSettings *settings = webkit_web_view_get_settings(instance->web_view);
  if (!settings)
  {
    return;
  }

  const gboolean js_enabled = flutter_map_get_bool(map, "javaScriptEnabled", TRUE);
  webkit_settings_set_enable_javascript(settings, js_enabled);

  const gboolean load_images = flutter_map_get_bool(map, "enableImages", TRUE);
  webkit_settings_set_auto_load_images(settings, load_images);

  const gboolean enable_zoom = flutter_map_get_bool(map, "enableZoom", FALSE);
  webkit_settings_set_zoom_text_only(settings, !enable_zoom);

  const gboolean media_gesture = flutter_map_get_bool(map, "mediaPlaybackRequiresUserGesture", TRUE);
  webkit_settings_set_media_playback_requires_user_gesture(settings, media_gesture);

  const gboolean save_form = flutter_map_get_bool(map, "saveFormData", FALSE);
  webkit_settings_set_enable_html5_local_storage(settings, save_form);

  // safeBrowsingEnabled: no WebKitGTK equivalent — intentionally ignored.
  // algorithmicDarkening / useWideViewPort: viewport/CSS handles most layout; no stable GTK API — ignored.

  gint min_font = flutter_map_get_int(map, "minimumFontSize", 8);
  if (min_font < 1)
  {
    min_font = 1;
  }
  if (min_font > 72)
  {
    min_font = 72;
  }
  webkit_settings_set_minimum_font_size(settings, min_font);

  gint text_zoom_pct = flutter_map_get_int(map, "textZoom", 100);
  if (text_zoom_pct < 50)
  {
    text_zoom_pct = 50;
  }
  if (text_zoom_pct > 200)
  {
    text_zoom_pct = 200;
  }
  const gint base_px = 16;
  const gint scaled = (base_px * text_zoom_pct + 50) / 100;
  webkit_settings_set_default_font_size(settings, scaled);
  const gint mono_scaled = (14 * text_zoom_pct + 50) / 100;
  webkit_settings_set_default_monospace_font_size(settings, mono_scaled > 6 ? mono_scaled : 6);

  webkit_settings_set_enable_smooth_scrolling(settings, TRUE);
  webkit_settings_set_enable_media_stream(settings, FALSE);
  webkit_settings_set_enable_webaudio(settings, FALSE);
  webkit_settings_set_enable_webgl(settings, FALSE);
}

void webview_webkitgtk_flutter_settings_install_handlers(WebViewWebKitGTK *instance)
{
  if (!instance || !instance->web_view)
  {
    return;
  }
  g_signal_connect(instance->web_view, "decide-policy",
                   G_CALLBACK(webview_flutter_on_decide_policy), instance);
  g_signal_connect(instance->web_view, "permission-request",
                   G_CALLBACK(webview_flutter_on_permission_request), instance);
}
