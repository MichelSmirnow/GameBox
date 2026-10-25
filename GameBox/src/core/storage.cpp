// ---------------------------------------------------------------------------
//  GameBox :: storage.cpp
// ---------------------------------------------------------------------------
#include "storage.h"
#include "config.h"
#include <string.h>

#ifdef ARDUINO
#include <Preferences.h>

Store::Store(const char* ns_) : ns(ns_) {}

bool Store::begin() {
  if (opened) return true;
  Preferences* p = new Preferences();
  if (!p->begin(ns, false)) { delete p; opened = false; return false; }
  handle = p;
  opened = true;
  return true;
}

void Store::end() {
  if (!opened) return;
  Preferences* p = (Preferences*)handle;
  p->end();
  delete p;
  handle = nullptr;
  opened = false;
}

uint32_t Store::getU32(const char* k, uint32_t def) {
  if (!opened && !begin()) return def;
  return ((Preferences*)handle)->getUInt(k, def);
}

void Store::setU32(const char* k, uint32_t v) {
  if (!opened && !begin()) return;
  ((Preferences*)handle)->putUInt(k, v);
}

float Store::getF(const char* k, float def) {
  if (!opened && !begin()) return def;
  return ((Preferences*)handle)->getFloat(k, def);
}

void Store::setF(const char* k, float v) {
  if (!opened && !begin()) return;
  ((Preferences*)handle)->putFloat(k, v);
}

size_t Store::getBytes(const char* k, void* dst, size_t len) {
  if (!opened && !begin()) return 0;
  return ((Preferences*)handle)->getBytes(k, dst, len);
}

void Store::setBytes(const char* k, const void* src, size_t len) {
  if (!opened && !begin()) return;
  ((Preferences*)handle)->putBytes(k, src, len);
}

void Store::remove(const char* k) {
  if (!opened && !begin()) return;
  ((Preferences*)handle)->remove(k);
}

bool Store::has(const char* k) {
  if (!opened && !begin()) return false;
  return ((Preferences*)handle)->isKey(k);
}

#else   // ---------------- хост-симуляция (для проверки логики) ----------------
#include <map>
#include <string>
#include <vector>
static std::map<std::string, std::vector<uint8_t>> g_mem;
static std::string key(const char* ns, const char* k) { return std::string(ns) + "/" + k; }

Store::Store(const char* ns_) : ns(ns_) {}
bool Store::begin() { opened = true; return true; }
void Store::end() { opened = false; }

uint32_t Store::getU32(const char* k, uint32_t def) {
  auto it = g_mem.find(key(ns, k));
  if (it == g_mem.end() || it->second.size() < 4) return def;
  uint32_t v; memcpy(&v, it->second.data(), 4); return v;
}
void Store::setU32(const char* k, uint32_t v) {
  std::vector<uint8_t> b(4); memcpy(b.data(), &v, 4); g_mem[key(ns, k)] = b;
}
float Store::getF(const char* k, float def) {
  auto it = g_mem.find(key(ns, k));
  if (it == g_mem.end() || it->second.size() < sizeof(float)) return def;
  float v; memcpy(&v, it->second.data(), sizeof(float)); return v;
}
void Store::setF(const char* k, float v) {
  std::vector<uint8_t> b(sizeof(float)); memcpy(b.data(), &v, sizeof(float)); g_mem[key(ns, k)] = b;
}
size_t Store::getBytes(const char* k, void* dst, size_t len) {
  auto it = g_mem.find(key(ns, k));
  if (it == g_mem.end()) return 0;
  size_t n = it->second.size() < len ? it->second.size() : len;
  memcpy(dst, it->second.data(), n);
  return n;
}
void Store::setBytes(const char* k, const void* src, size_t len) {
  const uint8_t* p = (const uint8_t*)src;
  g_mem[key(ns, k)] = std::vector<uint8_t>(p, p + len);
}
void Store::remove(const char* k) { g_mem.erase(key(ns, k)); }
bool Store::has(const char* k) { return g_mem.count(key(ns, k)) > 0; }
#endif
