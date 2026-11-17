#include "persistent_asset_cache.h"
#include "LittleFS.h"
#include "esp_heap_caps.h"
#include <mbedtls/sha256.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace leopold::asset_cache;
void require(bool value,const char* message) { if(!value) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
std::string hex(const std::vector<uint8_t>& data) { uint8_t d[32]; mbedtls_sha256(data.data(),data.size(),d,0); char out[65]; for(int i=0;i<32;++i) std::snprintf(out+i*2,3,"%02x",d[i]); return out; }
struct Reader { const std::vector<uint8_t>* data; size_t at=0; size_t stopAt=SIZE_MAX; };
bool readChunk(void* opaque,uint8_t* out,size_t want,size_t* got) { auto* r=static_cast<Reader*>(opaque); *got=0; if(r->at>=r->stopAt || r->at>=r->data->size()) return false; *got=std::min(want,std::min(r->stopAt-r->at,r->data->size()-r->at)); std::memcpy(out,r->data->data()+r->at,*got); r->at+=*got; return *got>0; }
AssetSpec spec(const char* id,const char* file,const std::vector<uint8_t>& bytes,uint16_t w,uint16_t h,const std::string& hash) { return {id,file,static_cast<uint32_t>(bytes.size()),hash.c_str(),w,h}; }

int main(int argc,char** argv) {
  require(argc==2,"mode required");
  const std::string mode=argv[1];
  require(begin(),"mount existing LittleFS without formatting");
  if(mode=="seed" || mode=="verify" || mode=="verify-corrupt") {
    std::vector<uint8_t> data(48); for(size_t i=0;i<data.size();++i)data[i]=static_cast<uint8_t>(i*7+3);
    auto digest=hex(data); auto a=spec("fixture","fixture.bin",data,4,4,digest);
    std::vector<uint8_t> second(48,0x5a); auto digest2=hex(second); auto b=spec("pending","pending.bin",second,4,4,digest2);
    if(mode=="seed") {
      require(install(a,data.data(),data.size(),Pin::kGlobalWallpaper),"install verified payload");
      require(install(b,second.data(),second.size(),Pin::kNone),"stage separate pending selection");
      require(setPin(a,Pin::kClockWallpaper),"pin the same immutable asset for Clock too");
      std::vector<uint8_t> bad=data; bad[8]^=0xff; Reader wrong{&bad};
      require(!installStream(a,readChunk,&wrong,Pin::kNone),"reject SHA mismatch");
      Reader truncated{&data,0,data.size()-1};
      require(!installStream(a,readChunk,&truncated,Pin::kNone),"reject interrupted/truncated transfer");
      Reader unavailable{&data,0,0};
      require(!installStream(a,readChunk,&unavailable,Pin::kNone),"reject unavailable host/empty stream");
      uint8_t* selected=nullptr; size_t size=0;
      require(select(a,static_cast<Pin>(3),&selected,&size) && size==data.size() && !std::memcmp(selected,data.data(),size),"selected buffer equals verified persistent payload");
      heap_caps_free(selected);
      require(persistentBytes()==data.size()+second.size(),"bounded persistent accounting");
      require(!LittleFS.exists("/asset-stage.tmp"),"failed attempt removes only its own incomplete staging file");
    } else if(mode=="verify") {
      std::vector<uint8_t> expected(48); for(size_t i=0;i<expected.size();++i)expected[i]=static_cast<uint8_t>(i*7+3);
      require(contains(a),"verified asset survives restart");
      uint8_t* selected=nullptr; size_t size=0;
      require(select(a,static_cast<Pin>(3),&selected,&size) && size==expected.size() && !std::memcmp(selected,expected.data(),size),"restart reselects both pinned wallpaper roles");
      heap_caps_free(selected);
      require(contains(b),"secondary content survives restart");
      require(LittleFS.exists("/asset-stage.tmp"),"interrupted staging is preserved across restart");
    } else {
      require(contains(a),"corrupt pending content does not damage previous pinned selection");
      require(!contains(b),"corrupt persistent content rejected on restart");
      const std::string corruptPath = "/a-" + digest2;
      require(LittleFS.exists(corruptPath.c_str()),"corrupt optional content is preserved for recovery");
    }
  } else if(mode=="evict") {
    const size_t n=465u*466u*3u;
    std::vector<uint8_t> aBytes(n,0x31), bBytes(n,0x52), cBytes(n,0x73);
    auto ah=hex(aBytes), bh=hex(bBytes), ch=hex(cBytes);
    auto a=spec("a","a.bin",aBytes,465,466,ah), b=spec("b","b.bin",bBytes,465,466,bh), c=spec("c","c.bin",cBytes,465,466,ch);
    require(install(a,aBytes.data(),aBytes.size(),Pin::kNone),"install first LRU candidate");
    require(install(b,bBytes.data(),bBytes.size(),Pin::kNone),"install second LRU candidate");
    require(install(c,cBytes.data(),cBytes.size(),Pin::kNone),"third entry evicts oldest unpinned item");
    require(!contains(a) && contains(b) && contains(c),"eviction preserves newer entries");
  } else if(mode=="pins") {
    const size_t n=465u*466u*3u;
    std::vector<uint8_t> aBytes(n,0x31), bBytes(n,0x52), cBytes(n,0x73);
    auto ah=hex(aBytes), bh=hex(bBytes), ch=hex(cBytes);
    auto a=spec("a","a.bin",aBytes,465,466,ah), b=spec("b","b.bin",bBytes,465,466,bh), c=spec("c","c.bin",cBytes,465,466,ch);
    require(install(a,aBytes.data(),aBytes.size(),Pin::kGlobalWallpaper),"install pinned global asset");
    require(install(b,bBytes.data(),bBytes.size(),Pin::kClockWallpaper),"install pinned Clock asset");
    require(!install(c,cBytes.data(),cBytes.size(),Pin::kNone),"storage exhaustion must not evict pinned choices");
    require(contains(a) && contains(b),"storage failure retains both pinned selections");
  } else if(mode=="transactional-rename" || mode=="transactional-index") {
    const size_t n=465u*466u*3u;
    std::vector<uint8_t> aBytes(n,0x21), bBytes(n,0x42), cBytes(n,0x63);
    auto ah=hex(aBytes), bh=hex(bBytes), ch=hex(cBytes);
    auto a=spec("a","a.bin",aBytes,465,466,ah), b=spec("b","b.bin",bBytes,465,466,bh), c=spec("c","c.bin",cBytes,465,466,ch);
    require(install(a,aBytes.data(),aBytes.size(),Pin::kNone),"seed first transactional entry");
    require(install(b,bBytes.data(),bBytes.size(),Pin::kNone),"seed second transactional entry");
    const std::string failTarget=mode=="transactional-rename" ? ("/a-"+ch) : "/asset-cache.json";
    setenv("LEOPOLD_TEST_FAIL_RENAME_TO",failTarget.c_str(),1);
    require(!install(c,cBytes.data(),cBytes.size(),Pin::kNone),"injected rename failure rejects optional install");
    unsetenv("LEOPOLD_TEST_FAIL_RENAME_TO");
    require(contains(a) && contains(b) && !contains(c),"failed install keeps prior verified selections");
    require(LittleFS.exists(("/a-"+ah).c_str()) && LittleFS.exists(("/a-"+bh).c_str()),"failed install retains prior files");
    require(persistentBytes()==aBytes.size()+bBytes.size(),"failed install restores prior index accounting");
  } else {
    require(false,"unknown mode");
  }
  std::puts("persistent firmware asset cache fixture passed");
}
