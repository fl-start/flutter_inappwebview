// Generic mapping from optional Flutter "settings" maps to WebKitGTK.
// SCOMM_LINUX_NETWORK_SEAL: mail WebViews fail closed. Missing maps / missing
// blockNetworkLoads keys default to network blocked. Remote http(s)/ws(s)
// subresources are denied via (1) scheme allowlist on decide-policy,
// (2) a dead HTTP proxy on the WebsiteDataManager (Soup path, including
// <img>/XHR), and (3) a WebKit content filter. Dart shouldInterceptRequest is
// not the Linux http(s) control plane.

#include "webview_webkitgtk_flutter_settings.h"

#include "webview_webkitgtk.h"

#include <glib.h>

namespace
{
  const char kNetworkBlockFilterId[] = "scomm-block-remote";
  const char kDeadProxyUri[] = "http://127.0.0.1:1";

  // Safari content-blocker JSON. Block all http(s)/ws(s) — mail content is
  // appmsg://local. Do not except loopback (mail HTML must not GET local ports).
  const char kRemoteBlockFilterJson[] =
      "["
      "{\"trigger\":{\"url-filter\":\"^https?://\"},\"action\":{\"type\":\"block\"}},"
      "{\"trigger\":{\"url-filter\":\"^wss?://\"},\"action\":{\"type\":\"block\"}}"
      "]";

  struct NavigationDecisionContext
  {
    WebKitPolicyDecision *decision;
  };

  struct ContentFilterApplyContext
  {
    WebKitWebView *web_view;
    WebKitUserContentManager *manager;
    WebKitUserContentFilterStore *store;
  };

  void on_navigation_policy_result(GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
  {
    auto *context = static_cast<NavigationDecisionContext *>(user_data);
    g_autoptr(GError) error = nullptr;
    g_autoptr(FlMethodResponse) response = fl_method_channel_invoke_method_finish(
        FL_METHOD_CHANNEL(source_object), result, &error);

    // No usable answer from Dart (channel error, no handler, wrong type):
    // refuse rather than let the page navigate unchecked.
    gint64 policy = 0;
    if (!error && FL_IS_METHOD_SUCCESS_RESPONSE(response))
    {
      FlValue *value = fl_method_success_response_get_result(
          FL_METHOD_SUCCESS_RESPONSE(response));
      if (value && fl_value_get_type(value) == FL_VALUE_TYPE_INT)
        policy = fl_value_get_int(value);
    }

    if (policy == 0)
      webkit_policy_decision_ignore(context->decision);
    else if (policy == 2)
      webkit_policy_decision_download(context->decision);
    else
      webkit_policy_decision_use(context->decision);

    g_object_unref(context->decision);
    delete context;
  }

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

  // The plugin's shared context serves every view that is neither incognito
  // nor network-blocked. Context-wide state (cache model, cookie policy,
  // proxy) set from one view's settings would silently apply to all of them,
  // so the shared context keeps WebKit's defaults.
  gboolean context_is_shared(WebKitWebContext *ctx)
  {
    return ctx != nullptr &&
           g_object_get_data(G_OBJECT(ctx), WEBVIEW_WEBKITGTK_SHARED_CONTEXT_KEY) != nullptr;
  }

  void apply_context_level_from_map(WebKitWebContext *ctx, FlValue *map)
  {
    if (!ctx || context_is_shared(ctx))
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

  // Fail-closed allowlist when blockNetworkLoads is on. Matches Dart
  // WebViewNetworkPolicy for mail surfaces (plus blob: for compose).
  // Loopback http(s) is not allowlisted — mail HTML must not GET local ports.
  gboolean uri_is_mail_allowlisted(const gchar *uri,
                                   gboolean allow_file,
                                   WebKitWebContext *ctx)
  {
    if (!uri || !*uri)
    {
      return FALSE;
    }
    // Any custom scheme this plugin serves (appmsg plus resourceCustomSchemes)
    // is local content, not network.
    if (ctx)
    {
      g_autofree gchar *scheme = g_uri_parse_scheme(uri);
      if (scheme)
      {
        g_autofree gchar *lower = g_ascii_strdown(scheme, -1);
        g_autofree gchar *marker =
            g_strdup_printf("scomm-uri-scheme-registered-%s", lower);
        if (g_object_get_data(G_OBJECT(ctx), marker) != nullptr)
          return TRUE;
      }
    }
    if (g_ascii_strncasecmp(uri, "about:", 6) == 0)
      return TRUE;
    if (g_ascii_strncasecmp(uri, "data:", 5) == 0)
      return TRUE;
    if (g_ascii_strncasecmp(uri, "blob:", 5) == 0)
      return TRUE;
    if (g_ascii_strncasecmp(uri, "appmsg:", 7) == 0)
      return TRUE;
    if (allow_file && g_ascii_strncasecmp(uri, "file:", 5) == 0)
      return TRUE;
    return FALSE;
  }

  void apply_dead_proxy_seal(WebKitWebContext *ctx, gboolean block)
  {
    // Network-blocked views get their own context at creation. A shared-
    // context view switched to blocked later relies on the policy check and
    // content filter; toggling the shared proxy would cut or unseal its peers.
    if (!ctx || context_is_shared(ctx))
    {
      return;
    }
    WebKitWebsiteDataManager *dm = webkit_web_context_get_website_data_manager(ctx);
    if (!dm)
    {
      return;
    }
    if (block)
    {
      WebKitNetworkProxySettings *ps =
          webkit_network_proxy_settings_new(kDeadProxyUri, nullptr);
      webkit_website_data_manager_set_network_proxy_settings(
          dm, WEBKIT_NETWORK_PROXY_MODE_CUSTOM, ps);
      webkit_network_proxy_settings_free(ps);
    }
    else
    {
      webkit_website_data_manager_set_network_proxy_settings(
          dm, WEBKIT_NETWORK_PROXY_MODE_DEFAULT, nullptr);
    }
  }

  void on_network_block_filter_saved(GObject *source_object,
                                     GAsyncResult *result,
                                     gpointer user_data)
  {
    auto *ctx = static_cast<ContentFilterApplyContext *>(user_data);
    g_autoptr(GError) error = nullptr;
    WebKitUserContentFilter *filter =
        webkit_user_content_filter_store_save_finish(
            WEBKIT_USER_CONTENT_FILTER_STORE(source_object), result, &error);
    if (error)
    {
      g_warning("Scomm WebKitGTK network content filter save failed: %s",
                error->message);
    }
    else if (ctx->web_view && ctx->manager && filter)
    {
      webkit_user_content_manager_remove_filter_by_id(ctx->manager,
                                                      kNetworkBlockFilterId);
      webkit_user_content_manager_add_filter(ctx->manager, filter);
    }
    if (filter)
      webkit_user_content_filter_unref(filter);
    if (ctx->web_view)
      g_object_remove_weak_pointer(G_OBJECT(ctx->web_view),
                                   reinterpret_cast<gpointer *>(&ctx->web_view));
    if (ctx->manager)
      g_object_unref(ctx->manager);
    if (ctx->store)
      g_object_unref(ctx->store);
    delete ctx;
  }

  void apply_content_filter_seal(WebViewWebKitGTK *instance, gboolean block)
  {
    if (!instance || !instance->user_content_manager)
    {
      return;
    }
    if (!block)
    {
      webkit_user_content_manager_remove_filter_by_id(
          instance->user_content_manager, kNetworkBlockFilterId);
      if (instance->web_view)
      {
        g_object_set_data(G_OBJECT(instance->web_view), "scomm-network-filter",
                          nullptr);
      }
      return;
    }
    if (!instance->web_view)
    {
      return;
    }
    if (g_object_get_data(G_OBJECT(instance->web_view), "scomm-network-filter") !=
        nullptr)
    {
      return;
    }
    g_object_set_data(G_OBJECT(instance->web_view), "scomm-network-filter",
                      GINT_TO_POINTER(1));

    g_autofree gchar *dir = g_build_filename(
        g_get_user_cache_dir(), "scomm-ai", "webkit-content-filters", nullptr);
    if (g_mkdir_with_parents(dir, 0700) != 0)
    {
      g_warning("Scomm WebKitGTK network content filter cache mkdir failed");
      g_object_set_data(G_OBJECT(instance->web_view), "scomm-network-filter",
                        nullptr);
      return;
    }

    WebKitUserContentFilterStore *store = webkit_user_content_filter_store_new(dir);
    auto *ctx = new ContentFilterApplyContext{
        instance->web_view, instance->user_content_manager, store};
    g_object_ref(ctx->manager);
    g_object_add_weak_pointer(G_OBJECT(ctx->web_view),
                              reinterpret_cast<gpointer *>(&ctx->web_view));
    GBytes *source = g_bytes_new_static(kRemoteBlockFilterJson,
                                        sizeof(kRemoteBlockFilterJson) - 1);
    webkit_user_content_filter_store_save(
        store, kNetworkBlockFilterId, source, nullptr,
        on_network_block_filter_saved, ctx);
    g_bytes_unref(source);
  }

  void apply_network_seal(WebViewWebKitGTK *instance, gboolean block)
  {
    apply_dead_proxy_seal(instance ? instance->web_context : nullptr, block);
    apply_content_filter_seal(instance, block);
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

    if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION &&
        inst->flutter_block_network_loads)
    {
      webkit_policy_decision_ignore(decision);
      return TRUE;
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

    if (inst->flutter_block_network_loads &&
        !uri_is_mail_allowlisted(uri, inst->flutter_allow_file_access,
                                 inst->web_context))
    {
      webkit_policy_decision_ignore(decision);
      return TRUE;
    }

    if (!inst->flutter_block_network_loads &&
        !inst->flutter_allow_file_access && g_str_has_prefix(uri, "file:"))
    {
      webkit_policy_decision_ignore(decision);
      return TRUE;
    }

    if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
        inst->method_channel)
    {
      g_autoptr(FlValue) args = fl_value_new_map();
      fl_value_set_string_take(args, "viewId",
                               fl_value_new_int((int64_t)inst->view_id));
      fl_value_set_string_take(args, "url", fl_value_new_string(uri));
      auto *context = new NavigationDecisionContext{
          WEBKIT_POLICY_DECISION(g_object_ref(decision))};
      fl_method_channel_invoke_method(
          inst->method_channel, "shouldOverrideUrlLoading", args, nullptr,
          on_navigation_policy_result, context);
      return TRUE;
    }

    return FALSE;
  }

  // "permission-request" returns gboolean; TRUE stops WebKit's default
  // handler, which would otherwise decide the request a second time.
  extern "C" gboolean webview_flutter_on_permission_request(WebKitWebView *web_view,
                                                            WebKitPermissionRequest *request,
                                                            gpointer user_data)
  {
    WebViewWebKitGTK *inst = static_cast<WebViewWebKitGTK *>(user_data);
    (void)web_view;
    if (!inst || !request)
    {
      return FALSE;
    }

    if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request) &&
        inst->flutter_geolocation_enabled)
    {
      webkit_permission_request_allow(request);
      return TRUE;
    }

    // Default-deny everything else for a hardened generic viewer.
    webkit_permission_request_deny(request);
    return TRUE;
  }

} // namespace

WebKitWebContext *webview_webkitgtk_create_context_from_flutter_settings(
    FlValue *map,
    WebKitWebContext *shared_context)
{
  const gboolean incognito = flutter_map_get_bool(map, "incognito", FALSE);
  // Same fail-closed default as webview_webkitgtk_apply_flutter_settings_map.
  // A blocked view needs its own data manager: the dead proxy is set per
  // manager, and on the shared one it would cut every network-allowed peer.
  const gboolean block_network =
      flutter_map_get_bool(map, "blockNetworkLoads", TRUE);

  WebKitWebContext *ctx = nullptr;
  if (incognito || block_network)
  {
    WebKitWebsiteDataManager *dm = webkit_website_data_manager_new_ephemeral();
    ctx = webkit_web_context_new_with_website_data_manager(dm);
    g_object_unref(dm);
    webview_webkitgtk_configure_context_sandbox(ctx);
  }
  else if (shared_context)
  {
    // The plugin configured the shared context's sandbox when it created it.
    ctx = WEBKIT_WEB_CONTEXT(g_object_ref(shared_context));
  }
  else
  {
    ctx = webkit_web_context_new();
    webview_webkitgtk_configure_context_sandbox(ctx);
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
    // SCOMM_LINUX_NETWORK_SEAL: missing map fails closed.
    instance->flutter_block_network_loads = TRUE;
    instance->flutter_allow_file_access = FALSE;
    instance->flutter_geolocation_enabled = FALSE;
    apply_context_level_from_map(instance->web_context, nullptr);
    apply_network_seal(instance, TRUE);
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
    // <a ping> would POST to a tracker on every link click.
    webkit_settings_set_enable_hyperlink_auditing(settings, FALSE);
    return;
  }

  apply_context_level_from_map(instance->web_context, map);

  instance->flutter_block_network_loads =
      flutter_map_get_bool(map, "blockNetworkLoads", TRUE);
  instance->flutter_allow_file_access = flutter_map_get_bool(map, "allowFileAccess", FALSE);
  instance->flutter_geolocation_enabled = flutter_map_get_bool(map, "geolocationEnabled", FALSE);
  apply_network_seal(instance, instance->flutter_block_network_loads);

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
  // <a ping> would POST to a tracker on every link click.
  webkit_settings_set_enable_hyperlink_auditing(settings, FALSE);
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
