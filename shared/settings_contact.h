#pragma once
#include <cstring>
namespace leopold {
class SettingsContact {
 public:
  void begin(const char* row) { active_ = true; cancelled_ = row == nullptr; row_ = row; }
  void move(const char* row) {
    if (active_ && (!row || !row_ || std::strcmp(row_, row) != 0)) cancelled_ = true;
  }
  const char* selected() const { return active_ && !cancelled_ ? row_ : nullptr; }
  const char* release(const char* row, bool allowed) {
    move(row);
    const char* action = allowed ? selected() : nullptr;
    active_ = false;
    return action;
  }
 private:
  bool active_ = false;
  bool cancelled_ = false;
  const char* row_ = nullptr;
};
} // namespace leopold
