#include "persistent_asset_cache.h"
#include "generated_selected_asset_specs.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>

#include "memory_budget.h"

#include <cstdio>
#include <cstring>

namespace leopold::asset_cache {
namespace {
constexpr char kIndex[] = "/asset-cache.json";
constexpr size_t kReserveBytes = 512 * 1024;
constexpr size_t kPsramRuntimeReserveBytes = 1536 * 1024;
constexpr size_t kMaxEntries = 12;
struct Entry { char id[48]; char file[80]; char hash[65]; uint32_t bytes; uint32_t used; uint8_t pin; };
Entry entries[kMaxEntries]{};
size_t entryCount = 0;
bool ready = false;

Entry* snapshotEntries() {
  auto* snapshot = static_cast<Entry*>(heap_caps_malloc(sizeof(entries), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (snapshot) memcpy(snapshot, entries, sizeof(entries));
  return snapshot;
}

bool validSpec(const AssetSpec& s) {
  return s.id && s.file && s.sha256 && strlen(s.id) < sizeof(Entry::id) &&
      strlen(s.file) < 64 && strlen(s.sha256) == 64 && s.bytes > 0 &&
      s.bytes <= kMaxAssetBytes && s.width && s.height &&
      static_cast<uint64_t>(s.width) * s.height * 3 == s.bytes;
}
bool digestMatches(const char* expected, const uint8_t digest[32]) {
  char actual[65]{};
  for (size_t i = 0; i < 32; ++i) snprintf(actual + i * 2, 3, "%02x", digest[i]);
  return strcmp(expected, actual) == 0;
}
bool verifyFile(const char* path, const char* expectedHash, size_t expectedBytes) {
  File f = LittleFS.open(path, "r"); if (!f || f.size() != expectedBytes) { if (f) f.close(); return false; }
  mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); mbedtls_sha256_starts(&ctx, 0);
  auto* block = static_cast<uint8_t*>(heap_caps_malloc(kIoChunkBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!block) { mbedtls_sha256_free(&ctx); f.close(); return false; }
  size_t total=0; bool ok=true;
  while (f.available()) { size_t n=f.read(block,kIoChunkBytes); if (!n) { ok=false; break; } mbedtls_sha256_update(&ctx,block,n); total+=n; }
  uint8_t digest[32]; mbedtls_sha256_finish(&ctx,digest); mbedtls_sha256_free(&ctx); f.close(); heap_caps_free(block);
  return ok && total==expectedBytes && digestMatches(expectedHash,digest);
}
void pathFor(const char* hash, char* out, size_t capacity, const char* suffix = "") {
  snprintf(out, capacity, "/a-%.64s%s", hash, suffix);
}
bool saveIndex() {
  char line[240];
  File f = LittleFS.open("/asset-cache.json.tmp", "w");
  if (!f || f.write(reinterpret_cast<const uint8_t*>("LEOPOLD_ASSET_CACHE_V1\n"), 23) != 23) {
    if (f) f.close(); LittleFS.remove("/asset-cache.json.tmp"); return false;
  }
  for (size_t i=0;i<entryCount;++i) {
    const int n=snprintf(line,sizeof(line),"%s|%s|%s|%lu|%lu|%u\n",entries[i].id,entries[i].file,entries[i].hash,
                         static_cast<unsigned long>(entries[i].bytes),static_cast<unsigned long>(entries[i].used),entries[i].pin);
    if(n<=0 || static_cast<size_t>(n)>=sizeof(line) || f.write(reinterpret_cast<const uint8_t*>(line),n)!=static_cast<size_t>(n)) {
      f.close(); LittleFS.remove("/asset-cache.json.tmp"); return false;
    }
  }
  f.flush(); f.close();
  LittleFS.remove("/asset-cache.json.old");
  if (LittleFS.exists(kIndex) && !LittleFS.rename(kIndex, "/asset-cache.json.old")) return false;
  if (!LittleFS.rename("/asset-cache.json.tmp", kIndex)) {
    if (LittleFS.exists("/asset-cache.json.old")) LittleFS.rename("/asset-cache.json.old", kIndex);
    return false;
  }
  LittleFS.remove("/asset-cache.json.old");
  return true;
}
bool loadIndex() {
  const char* indexPath = LittleFS.exists(kIndex) ? kIndex : "/asset-cache.json.old";
  File f = LittleFS.open(indexPath, "r");
  if (!f) return true;
  char line[240]; size_t length=0;
  while(f.available() && length+1<sizeof(line)) { const int c=f.read(); if(c<0) break; if(c=='\n') break; line[length++]=static_cast<char>(c); }
  line[length]='\0';
  if(strcmp(line,"LEOPOLD_ASSET_CACHE_V1")!=0) { f.close(); return false; }
  while(f.available()) {
    length=0;
    while(f.available() && length+1<sizeof(line)) { const int c=f.read(); if(c<0) break; if(c=='\n') break; line[length++]=static_cast<char>(c); }
    if(length==0) continue;
    line[length]='\0';
    if(entryCount>=kMaxEntries) { f.close(); return false; }
    Entry parsed{}; unsigned long bytes=0,used=0; unsigned pin=0;
    char* a=strchr(line,'|'); if(!a) continue; *a++='\0';
    char* b=strchr(a,'|'); if(!b) continue; *b++='\0';
    char* c=strchr(b,'|'); if(!c) continue; *c++='\0';
    char* d=strchr(c,'|'); if(!d) continue; *d++='\0';
    char* e=strchr(d,'|'); if(!e) continue; *e++='\0';
    if(sscanf(c,"%lu",&bytes)!=1 || sscanf(d,"%lu",&used)!=1 || sscanf(e,"%u",&pin)!=1 || pin>3) continue;
    strlcpy(parsed.id,line,sizeof(parsed.id)); strlcpy(parsed.file,a,sizeof(parsed.file)); strlcpy(parsed.hash,b,sizeof(parsed.hash));
    parsed.bytes=bytes; parsed.used=used; parsed.pin=pin;
    Entry& entry=entries[entryCount++]; entry=parsed;
    char path[96]; pathFor(entry.hash,path,sizeof(path));
    // A bad optional entry is ignored in memory. Preserve its bytes for
    // recovery/inspection; it cannot become active until verified again.
    if(strlen(entry.hash)!=64 || !verifyFile(path,entry.hash,entry.bytes)) { --entryCount; continue; }
  }
  f.close();
  return true;
}
size_t usedBytes() { size_t n = 0; for (size_t i=0;i<entryCount;++i) n += entries[i].bytes; return n; }
int findHash(const char* hash) { for (size_t i=0;i<entryCount;++i) if (!strcmp(entries[i].hash, hash)) return static_cast<int>(i); return -1; }
int evictionCandidate(int excluded) {
  int candidate = -1;
  for (size_t i=0;i<entryCount;++i) if (static_cast<int>(i) != excluded && entries[i].pin == 0 &&
      (candidate < 0 || entries[i].used < entries[candidate].used)) candidate = static_cast<int>(i);
  return candidate;
}
void forgetEntry(size_t i) {
  for (size_t j=i+1;j<entryCount;++j) entries[j-1]=entries[j];
  if (entryCount) --entryCount;
}
}  // namespace

bool begin() {
  if (ready) return true;
  ready = loadIndex();
  // Preserve interrupted staging and previous-index files for recovery. They
  // are never treated as active artwork unless the verified index selects it.
  return ready;
}
bool mounted() { return ready; }
size_t persistentBytes() { return ready ? usedBytes() : 0; }
const AssetSpec* findSpec(const char* file) {
  if (!file) return nullptr;
  for (const AssetSpec& spec : kSelectedWallpapers) if (!strcmp(spec.file,file)) return &spec;
  return nullptr;
}
const AssetSpec* selectedWallpaper(uint8_t index) {
  return index < sizeof(kSelectedWallpapers)/sizeof(kSelectedWallpapers[0]) ? &kSelectedWallpapers[index] : nullptr;
}
bool contains(const AssetSpec& spec) {
  const int i=findHash(spec.sha256);
  if (i<0 || entries[i].bytes!=spec.bytes) return false;
  char path[96]; pathFor(spec.sha256,path,sizeof(path));
  return verifyFile(path,spec.sha256,spec.bytes);
}
bool setPin(const AssetSpec& spec, Pin pin) {
  const int i=findHash(spec.sha256); if(i<0 || entries[i].bytes!=spec.bytes) return false;
  Entry* before = snapshotEntries(); if (!before) return false;
  const uint8_t bit=static_cast<uint8_t>(pin);
  if(bit) for(size_t j=0;j<entryCount;++j) entries[j].pin &= static_cast<uint8_t>(~bit);
  entries[i].pin |= bit; entries[i].used=millis();
  const bool saved = saveIndex();
  if(!saved) memcpy(entries,before,sizeof(entries));
  heap_caps_free(before);
  return saved;
}

bool installStream(const AssetSpec& spec, bool (*read)(void*, uint8_t*, size_t, size_t*), void* context, Pin pin) {
  if (!ready || !validSpec(spec) || read == nullptr) return false;
  char stage[96], finalPath[96]; pathFor(spec.sha256, finalPath, sizeof(finalPath));
  strlcpy(stage, "/asset-stage.tmp", sizeof(stage));
  const size_t reliableBytes = (LittleFS.totalBytes() / 4) * 3;
  if (LittleFS.usedBytes() > reliableBytes ||
      spec.bytes + kReserveBytes > reliableBytes - LittleFS.usedBytes()) return false;
  Serial.printf("LEOPOLD_STORAGE phase=asset-stage used=%lu total=%lu staging=%lu reserve=%lu cache=%lu\n",
                static_cast<unsigned long>(LittleFS.usedBytes()), static_cast<unsigned long>(LittleFS.totalBytes()),
                static_cast<unsigned long>(spec.bytes), static_cast<unsigned long>(kReserveBytes),
                static_cast<unsigned long>(kCacheLimitBytes));
  File out = LittleFS.open(stage, "w"); if (!out) return false;
  mbedtls_sha256_context ctx; mbedtls_sha256_init(&ctx); mbedtls_sha256_starts(&ctx, 0);
  auto* buffer = static_cast<uint8_t*>(heap_caps_malloc(kIoChunkBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!buffer) { mbedtls_sha256_free(&ctx); out.close(); LittleFS.remove(stage); return false; }
  size_t total = 0; bool ok = true;
  while (total < spec.bytes) {
    size_t got = 0; const size_t want = min(kIoChunkBytes, static_cast<size_t>(spec.bytes - total));
    if (!read(context, buffer, want, &got) || got == 0 || got > want || out.write(buffer, got) != got) { ok=false; break; }
    mbedtls_sha256_update(&ctx, buffer, got); total += got;
  }
  uint8_t digest[32]; mbedtls_sha256_finish(&ctx, digest); mbedtls_sha256_free(&ctx);
  heap_caps_free(buffer);
  out.flush(); out.close();
  if (!ok || total != spec.bytes || !digestMatches(spec.sha256, digest)) { LittleFS.remove(stage); return false; }
  int existing = findHash(spec.sha256);
  size_t projected = usedBytes() - (existing >= 0 ? entries[existing].bytes : 0) + spec.bytes;
  size_t reclaimable = 0;
  size_t reclaimableCount = 0;
  for (size_t i = 0; i < entryCount; ++i)
    if (entries[i].pin == 0 && static_cast<int>(i) != existing) {
      reclaimable += entries[i].bytes;
      ++reclaimableCount;
    }
  if ((projected > kCacheLimitBytes && projected - reclaimable > kCacheLimitBytes) ||
      (existing < 0 && entryCount >= kMaxEntries && reclaimableCount == 0)) {
    LittleFS.remove(stage);
    return false;
  }
  // Remove only eligible LRU entries. Pinned selections and active leases are
  // not evicted; current renderer buffers are RAM copies and survive rename.
  Entry* before = snapshotEntries();
  if (!before) { LittleFS.remove(stage); return false; }
  const size_t countBefore = entryCount;
  char evictedPaths[kMaxEntries][96]{};
  size_t evictedCount = 0;
  while ((usedBytes() - (existing >= 0 ? entries[existing].bytes : 0) + spec.bytes) > kCacheLimitBytes ||
         (existing < 0 && entryCount >= kMaxEntries)) {
    int victim = evictionCandidate(existing); if (victim < 0) {
      memcpy(entries, before, sizeof(entries)); entryCount=countBefore;
      heap_caps_free(before); LittleFS.remove(stage); return false;
    }
    pathFor(entries[victim].hash, evictedPaths[evictedCount++], sizeof(evictedPaths[0]));
    forgetEntry(static_cast<size_t>(victim));
    if (existing > victim) --existing;
  }
  // Existing content is already address-verified and hashes to the same path;
  // only its index metadata needs updating. For an unindexed leftover at the
  // target path, move it to a recovery name and preserve it on successful
  // install. Never erase an unverified optional file to make room.
  const bool replacingExisting = existing >= 0;
  char recoveryPath[112]{};
  bool displacedOrphan = false;
  if (!replacingExisting) {
    if (LittleFS.exists(finalPath)) {
      snprintf(recoveryPath, sizeof(recoveryPath), "%s.recovery", finalPath);
      if (LittleFS.exists(recoveryPath) || !LittleFS.rename(finalPath, recoveryPath)) {
        memcpy(entries, before, sizeof(entries)); entryCount=countBefore;
        heap_caps_free(before); LittleFS.remove(stage); return false;
      }
      displacedOrphan = true;
    }
    if (!LittleFS.rename(stage, finalPath)) {
      if (displacedOrphan) LittleFS.rename(recoveryPath, finalPath);
      memcpy(entries, before, sizeof(entries)); entryCount=countBefore;
      heap_caps_free(before); LittleFS.remove(stage); return false;
    }
  }
  Entry next{}; strlcpy(next.id,spec.id,sizeof(next.id)); strlcpy(next.file,spec.file,sizeof(next.file));
  strlcpy(next.hash,spec.sha256,sizeof(next.hash)); next.bytes=spec.bytes; next.used=millis();
  next.pin=static_cast<uint8_t>((existing >= 0 ? entries[existing].pin : 0) | static_cast<uint8_t>(pin));
  if (existing >= 0) entries[existing]=next;
  else if (entryCount < kMaxEntries) entries[entryCount++]=next;
  else {
    memcpy(entries, before, sizeof(entries)); entryCount=countBefore;
    heap_caps_free(before);
    if (!replacingExisting) LittleFS.remove(finalPath);
    if (displacedOrphan) LittleFS.rename(recoveryPath, finalPath);
    LittleFS.remove(stage); return false;
  }
  if (!saveIndex()) {
    memcpy(entries, before, sizeof(entries)); entryCount=countBefore;
    heap_caps_free(before);
    if (!replacingExisting) LittleFS.remove(finalPath);
    if (displacedOrphan) LittleFS.rename(recoveryPath, finalPath);
    LittleFS.remove(stage); return false;
  }
  heap_caps_free(before);
  if (replacingExisting) LittleFS.remove(stage);
  // Deletion is the last step, after the new index is durable. Any earlier
  // failure leaves every previously indexed entry and file available.
  for (size_t i = 0; i < evictedCount; ++i) LittleFS.remove(evictedPaths[i]);
  return true;
}
struct MemoryReader { const uint8_t* p; size_t n; size_t at; };
static bool memoryRead(void* context,uint8_t* out,size_t want,size_t* got) { auto* r=static_cast<MemoryReader*>(context); if (!r || r->at >= r->n) return false; *got=min(want,r->n-r->at); memcpy(out,r->p+r->at,*got); r->at+=*got; return true; }
bool install(const AssetSpec& spec,const uint8_t* data,size_t length,Pin pin) {
  if (!data || length != spec.bytes) return false; MemoryReader reader{data,length,0};
  return installStream(spec,memoryRead,&reader,pin);
}
bool select(const AssetSpec& spec, Pin pin, uint8_t** data, size_t* length) {
  if (!ready || !validSpec(spec) || !data || !length) return false;
  int i=findHash(spec.sha256); if (i<0 || entries[i].bytes!=spec.bytes) return false;
  char path[96]; pathFor(spec.sha256,path,sizeof(path)); File f=LittleFS.open(path,"r"); if (!f) return false;
  const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const size_t freeBytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (largest < spec.bytes || freeBytes < static_cast<size_t>(spec.bytes) + kPsramRuntimeReserveBytes) {
    Serial.printf("LEOPOLD_ASSET_ERROR memory_budget bytes=%lu largest=%lu free=%lu reserve=%lu\n",
                  static_cast<unsigned long>(spec.bytes), static_cast<unsigned long>(largest),
                  static_cast<unsigned long>(freeBytes), static_cast<unsigned long>(kPsramRuntimeReserveBytes));
    return false;
  }
  memory_budget::log("asset-select-before");
  uint8_t* buffer=static_cast<uint8_t*>(heap_caps_malloc(spec.bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if (!buffer) { f.close(); return false; }
  size_t got=f.read(buffer,spec.bytes); f.close();
  uint8_t digest[32]; mbedtls_sha256(buffer,got,digest,0);
  if (got!=spec.bytes || !digestMatches(spec.sha256,digest)) { heap_caps_free(buffer); return false; }
  Entry* before = snapshotEntries();
  if (!before) { heap_caps_free(buffer); return false; }
  const uint8_t bit=static_cast<uint8_t>(pin);
  if (bit) for (size_t j=0;j<entryCount;++j) entries[j].pin &= static_cast<uint8_t>(~bit);
  entries[i].id[0]='\0'; strlcpy(entries[i].id,spec.id,sizeof(entries[i].id));
  strlcpy(entries[i].file,spec.file,sizeof(entries[i].file)); entries[i].pin |= bit; entries[i].used=millis();
  if (!saveIndex()) { memcpy(entries,before,sizeof(entries)); heap_caps_free(before); heap_caps_free(buffer); return false; }
  heap_caps_free(before);
  *data=buffer; *length=got;
  memory_budget::log("asset-select-after");
  return true;
}
}  // namespace leopold::asset_cache
