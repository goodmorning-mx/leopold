#pragma once
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

class File {
 public:
  File() = default;
  explicit File(FILE* file) : f_(file) {}
  File(const File&) = delete;
  File& operator=(const File&) = delete;
  File(File&& other) noexcept : f_(other.f_) { other.f_ = nullptr; }
  File& operator=(File&& other) noexcept { if (this != &other) { close(); f_=other.f_; other.f_=nullptr; } return *this; }
  ~File() { close(); }
  explicit operator bool() const { return f_ != nullptr; }
  size_t size() { if (!f_) return 0; long old=std::ftell(f_); std::fseek(f_,0,SEEK_END); long n=std::ftell(f_); std::fseek(f_,old,SEEK_SET); return n < 0 ? 0 : static_cast<size_t>(n); }
  int available() { return f_ && !std::feof(f_); }
  int read() { return f_ ? std::fgetc(f_) : -1; }
  size_t read(uint8_t* out, size_t n) { return f_ ? std::fread(out,1,n,f_) : 0; }
  size_t write(const uint8_t* data, size_t n) { return f_ ? std::fwrite(data,1,n,f_) : 0; }
  void flush() { if (f_) std::fflush(f_); }
  void close() { if (f_) { std::fclose(f_); f_=nullptr; } }
 private:
  FILE* f_ = nullptr;
};

class LittleFSFS {
 public:
  bool begin(bool) { std::filesystem::create_directories(root()); return true; }
  File open(const char* path, const char* mode) {
    const auto p = root() / (path[0]=='/' ? path+1 : path);
    if (mode[0]=='w') std::filesystem::create_directories(p.parent_path());
    return File(std::fopen(p.string().c_str(),mode));
  }
  bool exists(const char* path) const { return std::filesystem::exists(root()/(path[0]=='/'?path+1:path)); }
  bool remove(const char* path) { std::error_code ec; std::filesystem::remove(root()/(path[0]=='/'?path+1:path),ec); return !ec; }
  bool rename(const char* from,const char* to) {
    const char* failTarget=std::getenv("LEOPOLD_TEST_FAIL_RENAME_TO");
    if(failTarget && std::string(to)==failTarget) return false;
    std::error_code ec; std::filesystem::rename(root()/(from[0]=='/'?from+1:from),root()/(to[0]=='/'?to+1:to),ec); return !ec;
  }
  size_t totalBytes() const { return 3538944; }
  size_t usedBytes() const { size_t n=0; if(std::filesystem::exists(root())) for(const auto& f:std::filesystem::recursive_directory_iterator(root())) if(f.is_regular_file()) n+=f.file_size(); return n; }
 private:
  std::filesystem::path root() const { const char* p=std::getenv("LEOPOLD_TEST_FS"); return p ? p : "/tmp/leopold-asset-cache-test"; }
};
inline LittleFSFS LittleFS;
