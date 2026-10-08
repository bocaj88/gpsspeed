#pragma once
#include "Arduino.h"
struct Preferences { bool begin(const char*,bool){return true;} void end(){} void clear(){}
 float getFloat(const char*,float d){return d;} bool getBool(const char*,bool d){return d;} uint8_t getUChar(const char*,uint8_t d){return d;}
 uint16_t getUShort(const char*,uint16_t d){return d;} String getString(const char*,const char*d){return String(d);} size_t getBytes(const char*,void*,size_t){return 0;}
 void putFloat(const char*,float){} void putBool(const char*,bool){} void putUChar(const char*,uint8_t){} void putUShort(const char*,uint16_t){} void putString(const char*,const char*){} void putBytes(const char*,const void*,size_t){} };
