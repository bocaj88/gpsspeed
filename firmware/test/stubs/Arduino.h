#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>
using std::min; using std::max;
template<class T> T constrain(T v, T a, T b){ return v<a?a:(v>b?b:v); }
inline size_t strlcpy(char*d,const char*s,size_t n){ size_t l=strlen(s); if(n){ size_t c=l<n-1?l:n-1; memcpy(d,s,c); d[c]=0;} return l; }
struct String : std::string { String(const char*s=""):std::string(s){} String(const std::string&s):std::string(s){} const char* c_str()const{return std::string::c_str();} };
#define ESP_ERROR_CHECK(x) (x)
