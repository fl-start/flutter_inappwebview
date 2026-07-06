import 'package:flutter/services.dart';
import 'package:flutter_inappwebview_platform_interface/flutter_inappwebview_platform_interface.dart';

/// Object specifying creation parameters for creating a [LinuxWebStorageManager].
///
/// When adding additional fields make sure they can be null or have a default
/// value to avoid breaking changes. See [PlatformWebStorageManagerCreationParams] for
/// more information.
class LinuxWebStorageManagerCreationParams
    extends PlatformWebStorageManagerCreationParams {
  /// Creates a new [LinuxWebStorageManagerCreationParams] instance.
  const LinuxWebStorageManagerCreationParams();

  /// Creates a [LinuxWebStorageManagerCreationParams] instance based on [PlatformWebStorageManagerCreationParams].
  factory LinuxWebStorageManagerCreationParams.fromPlatformWebStorageManagerCreationParams(
    PlatformWebStorageManagerCreationParams params,
  ) {
    return const LinuxWebStorageManagerCreationParams();
  }
}

/// Auxiliary [PlatformWebStorageManager] for Linux (WebKitGTK channel API).
class LinuxWebStorageManager extends PlatformWebStorageManager {
  static const MethodChannel _channel = MethodChannel(
    'com.pichillilorenzo/flutter_inappwebview_webstoragemanager',
  );

  /// Constructs a [LinuxWebStorageManager].
  LinuxWebStorageManager(PlatformWebStorageManagerCreationParams params)
    : super.implementation(
        params is LinuxWebStorageManagerCreationParams
            ? params
            : LinuxWebStorageManagerCreationParams.fromPlatformWebStorageManagerCreationParams(
                params,
              ),
      );

  static final LinuxWebStorageManager _instance = LinuxWebStorageManager(
    const LinuxWebStorageManagerCreationParams(),
  );

  /// The [LinuxWebStorageManager] singleton instance.
  static LinuxWebStorageManager instance() => _instance;

  /// Creates and returns a new [LinuxWebStorageManager] for static methods.
  factory LinuxWebStorageManager.static() => _instance;

  /// Maps WebsiteDataType to WebKitGTK website-data type strings.
  static String _toWebKitWebsiteDataType(WebsiteDataType type) {
    if (type == WebsiteDataType.WKWebsiteDataTypeDiskCache) {
      return 'WEBKIT_WEBSITE_DATA_DISK_CACHE';
    } else if (type == WebsiteDataType.WKWebsiteDataTypeMemoryCache) {
      return 'WEBKIT_WEBSITE_DATA_MEMORY_CACHE';
    } else if (type ==
        WebsiteDataType.WKWebsiteDataTypeOfflineWebApplicationCache) {
      return 'WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE';
    } else if (type == WebsiteDataType.WKWebsiteDataTypeCookies) {
      return 'WEBKIT_WEBSITE_DATA_COOKIES';
    } else if (type == WebsiteDataType.WKWebsiteDataTypeSessionStorage) {
      return 'WEBKIT_WEBSITE_DATA_SESSION_STORAGE';
    } else if (type == WebsiteDataType.WKWebsiteDataTypeLocalStorage) {
      return 'WEBKIT_WEBSITE_DATA_LOCAL_STORAGE';
    } else if (type == WebsiteDataType.WKWebsiteDataTypeIndexedDBDatabases) {
      return 'WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES';
    } else if (type ==
        WebsiteDataType.WKWebsiteDataTypeServiceWorkerRegistrations) {
      return 'WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS';
    } else if (type == WebsiteDataType.WKWebsiteDataTypeFetchCache) {
      return 'WEBKIT_WEBSITE_DATA_DISK_CACHE';
    } else if (type == WebsiteDataType.WKWebsiteDataTypeWebSQLDatabases) {
      // WebSQL is deprecated, map to local storage
      return 'WEBKIT_WEBSITE_DATA_LOCAL_STORAGE';
    }
    // Default fallback
    return 'WEBKIT_WEBSITE_DATA_LOCAL_STORAGE';
  }

  /// Maps WebKitGTK website-data type string to [WebsiteDataType].
  static WebsiteDataType? _fromWebKitWebsiteDataType(String webkitType) {
    if (webkitType == 'WEBKIT_WEBSITE_DATA_DISK_CACHE') {
      return WebsiteDataType.WKWebsiteDataTypeDiskCache;
    } else if (webkitType == 'WEBKIT_WEBSITE_DATA_MEMORY_CACHE') {
      return WebsiteDataType.WKWebsiteDataTypeMemoryCache;
    } else if (webkitType == 'WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE') {
      return WebsiteDataType.WKWebsiteDataTypeOfflineWebApplicationCache;
    } else if (webkitType == 'WEBKIT_WEBSITE_DATA_COOKIES') {
      return WebsiteDataType.WKWebsiteDataTypeCookies;
    } else if (webkitType == 'WEBKIT_WEBSITE_DATA_SESSION_STORAGE') {
      return WebsiteDataType.WKWebsiteDataTypeSessionStorage;
    } else if (webkitType == 'WEBKIT_WEBSITE_DATA_LOCAL_STORAGE') {
      return WebsiteDataType.WKWebsiteDataTypeLocalStorage;
    } else if (webkitType == 'WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES') {
      return WebsiteDataType.WKWebsiteDataTypeIndexedDBDatabases;
    } else if (webkitType == 'WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS') {
      return WebsiteDataType.WKWebsiteDataTypeServiceWorkerRegistrations;
    }
    return null;
  }

  @override
  Future<List<WebsiteDataRecord>> fetchDataRecords({
    required Set<WebsiteDataType> dataTypes,
  }) async {
    final List<String> webkitDataTypes = dataTypes
        .map((type) => _toWebKitWebsiteDataType(type))
        .toList();

    final result = await _channel.invokeMethod<List<dynamic>>(
      'fetchDataRecords',
      {'dataTypes': webkitDataTypes},
    );

    if (result == null) {
      return [];
    }

    return result.cast<Map<dynamic, dynamic>>().map((recordMap) {
      final String? displayName = recordMap['displayName'] as String?;
      final List<dynamic>? dataTypesRaw =
          recordMap['dataTypes'] as List<dynamic>?;

      Set<WebsiteDataType>? dataTypesSet;
      if (dataTypesRaw != null) {
        dataTypesSet = dataTypesRaw
            .cast<String>()
            .map((typeStr) => _fromWebKitWebsiteDataType(typeStr))
            .whereType<WebsiteDataType>()
            .toSet();
      }

      return WebsiteDataRecord(
        displayName: displayName,
        dataTypes: dataTypesSet,
      );
    }).toList();
  }

  @override
  Future<void> removeDataFor({
    required Set<WebsiteDataType> dataTypes,
    required List<WebsiteDataRecord> dataRecords,
  }) async {
    final List<String> webkitDataTypes = dataTypes
        .map((type) => _toWebKitWebsiteDataType(type))
        .toList();

    final List<Map<String, dynamic>> recordList = dataRecords.map((record) {
      return {
        'displayName': record.displayName,
        'dataTypes': record.dataTypes
            ?.map((type) => _toWebKitWebsiteDataType(type))
            .toList(),
      };
    }).toList();

    await _channel.invokeMethod('removeDataFor', {
      'dataTypes': webkitDataTypes,
      'recordList': recordList,
    });
  }

  @override
  Future<void> removeDataModifiedSince({
    required Set<WebsiteDataType> dataTypes,
    required DateTime date,
  }) async {
    final List<String> webkitDataTypes = dataTypes
        .map((type) => _toWebKitWebsiteDataType(type))
        .toList();

    // Convert DateTime to Unix timestamp (seconds since epoch)
    final int timestamp = date.millisecondsSinceEpoch ~/ 1000;

    await _channel.invokeMethod('removeDataModifiedSince', {
      'dataTypes': webkitDataTypes,
      'timestamp': timestamp,
    });
  }
}
