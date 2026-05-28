#ifndef GEOCODING_H
#define GEOCODING_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

struct Location {
    float latitude;
    float longitude;
    String city;
    String country;
    String timezone;
    bool valid;
};

class Geocoding {
public:
    static Location searchCity(const String& cityName);
    static Location searchCityWithCountry(const String& cityName, const String& countryCode = "");
    
private:
    static Location parseNominatimResponse(const String& json);
    static String buildNominatimPath(const String& cityName, const String& countryCode);
};

#endif // GEOCODING_H

