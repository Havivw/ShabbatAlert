#include "geocoding.h"
#include "logger.h"
#include <ESP8266WiFi.h>

Location Geocoding::searchCity(const String& cityName) {
    return searchCityWithCountry(cityName, "");
}

Location Geocoding::searchCityWithCountry(const String& cityName, const String& countryCode) {
    Location result;
    result.valid = false;

    if (WiFi.status() != WL_CONNECTED) {
        LOG("Cannot geocode: WiFi not connected");
        return result;
    }

    String pathQuery = buildNominatimPath(cityName, countryCode);
    LOGF("Geocoding: https://nominatim.openstreetmap.org%s", pathQuery.c_str());
    // Nominatim redirects HTTP -> HTTPS, so we go straight to TLS.
    // Minimum TLS buffers (512) to fit in the ESP8266's small free heap.
    WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(512, 512);
    HTTPClient http;
    const char* host = "nominatim.openstreetmap.org";
    http.begin(client, host, 443, pathQuery, true);

    http.setTimeout(15000);
    // Nominatim requires a valid User-Agent identifying the application
    http.addHeader("User-Agent", "ShabbatAlert/1.0 (ESP8266; shabbat-times-alert)");
    // Force English responses so country names match the string checks below
    // (e.g. "Israel" rather than localised "ישראל"/"إسرائيل" → otherwise the
    // timezone falls through to UTC and Israel-mode detection has to lean
    // entirely on the lat/lon fallback).
    http.addHeader("Accept-Language", "en");
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        LOGF("Geocoding API error: %d", httpCode);
        http.end();
        return result;
    }
    
    String payload = http.getString();
    http.end();
    
    result = parseNominatimResponse(payload);
    
    if (result.valid) {
        LOGF("Found: %s, %s at %.6f, %.6f", 
             result.city.c_str(), result.country.c_str(), 
             result.latitude, result.longitude);
    } else {
        LOG("City not found");
    }
    
    return result;
}

String Geocoding::buildNominatimPath(const String& cityName, const String& countryCode) {
    String path = "/search?q=";
    String encoded = cityName;
    encoded.replace(" ", "+");
    path += encoded;
    if (countryCode.length() > 0) {
        path += "+" + countryCode;
    }
    path += "&format=json&limit=1&addressdetails=1";
    return path;
}

Location Geocoding::parseNominatimResponse(const String& json) {
    Location result;
    result.valid = false;
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        LOGF("JSON parse error: %s", error.c_str());
        return result;
    }
    
    if (!doc.is<JsonArray>() || doc.size() == 0) {
        LOG("No results found");
        return result;
    }
    
    JsonObject first = doc[0];
    
    // Extract coordinates
    if (first.containsKey("lat") && first.containsKey("lon")) {
        result.latitude = first["lat"].as<float>();
        result.longitude = first["lon"].as<float>();
    } else {
        return result;
    }
    
    // Extract address components
    if (first.containsKey("address")) {
        JsonObject address = first["address"];
        
        // Try to get city name from various fields
        if (address.containsKey("city")) {
            result.city = address["city"].as<String>();
        } else if (address.containsKey("town")) {
            result.city = address["town"].as<String>();
        } else if (address.containsKey("village")) {
            result.city = address["village"].as<String>();
        } else if (address.containsKey("municipality")) {
            result.city = address["municipality"].as<String>();
        } else if (first.containsKey("display_name")) {
            // Fallback: use display name
            String displayName = first["display_name"].as<String>();
            int commaPos = displayName.indexOf(',');
            if (commaPos > 0) {
                result.city = displayName.substring(0, commaPos);
            } else {
                result.city = displayName;
            }
        }
        
        // Get country
        if (address.containsKey("country")) {
            result.country = address["country"].as<String>();
        }
    }
    
    // Try to infer timezone from country/region
    // This is a simplified approach - for production, use a proper timezone API
    String country = result.country;
    if (country.indexOf("Israel") >= 0 || country.indexOf("Palestine") >= 0) {
        result.timezone = "Asia/Jerusalem";
    } else if (country.indexOf("United States") >= 0 || country.indexOf("USA") >= 0) {
        // Default to Eastern Time - could be improved with state detection
        result.timezone = "America/New_York";
    } else if (country.indexOf("United Kingdom") >= 0 || country.indexOf("UK") >= 0) {
        result.timezone = "Europe/London";
    } else {
        // Default timezone - user can change in settings
        result.timezone = "UTC";
    }
    
    result.valid = true;
    return result;
}

