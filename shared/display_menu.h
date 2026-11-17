#pragma once
// One screen model for the ESP32 and the existing LVGL WebAssembly simulator.
// Platform adapters supply preferences and perform I/O; no Arduino/LVGL calls here.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdui.h"
#include "generated_firmware_ui.h"
namespace leopold::display_menu {
  struct State {
    int expanded = -1;
    int scroll = 0;
    int background = 3;
    int backgroundGalleryOffset = 3;
    bool dark = true;
    int accent = 4;
    bool metric = true;
    bool spanish = false;
    int country = 0;
    // 0 unset, 1 US, 2 MX; selection is not a verified location.
    bool keypad = false;
    char location[97] = {};
    char postal[16] = {};
    char status[97] = {};
    bool clock24 = true;
    int clockFace = 0;
    int clockBackground = 0;
    bool clockWeather = false;
  };
  enum class Effect {
    kNone, kBack, kClock, kPreferences, kLookup
  };
  struct Row {
    const char* key;
    const char* english;
    const char* spanish;
  };
  inline constexpr Row kRows[] = {
   {
      "background", "Background", "Fondo"
    }, {
      "appearance", "Appearance", "Apariencia"
    },
   {
      "clock", "Clock", "Reloj"
    }, {
      "units", "Units of measurement", "Unidades de medida"
    },
   {
      "language", "Language", "Idioma"
    }, {
      "weather-location", "Weather Location", "Ubicación del clima"
    },
  };
  inline constexpr const char* kAccentKeys[] = {
    "red", "orange", "yellow", "green", "blue", "indigo", "violet"
  };
  inline constexpr const char* kAccentLabels[] = {
    "Red", "Orange", "Yellow", "Green", "Blue", "Indigo", "Violet"
  };
  inline constexpr int kViewportX = generated_ui::kDisplayMenuViewportX;
  inline constexpr int kViewportY = generated_ui::kDisplayMenuViewportY;
  inline constexpr int kViewportWidth = generated_ui::kDisplayMenuViewportWidth;
  inline constexpr int kViewportHeight = generated_ui::kDisplayMenuViewportHeight;
  inline constexpr int kBackgroundCount = 4;

  inline int clamp(int n, int low, int high) {
    return n < low ? low : n > high ? high : n;
  }

  inline void copy(char* out, size_t capacity, const char* value) {
    snprintf(out, capacity, "%s", value ? value : "");
  }

  inline int rowIndex(const char* key) {
    for (int i = 0; i < 6; ++i) if (key && strcmp(key, kRows[i].key) == 0) return i;
    return -1;
  }

  inline int expansion(const State& s) {
    switch (s.expanded) {
      case 0: return 145;
      case 1: return 120;
      case 3: case 4: return 50;
      case 5: return s.country && s.keypad ? 380 : 86;
      default: return 0;
    }
  }

  inline int maxScroll(const State& s) {
    const int total = 6 * (generated_ui::kDisplayMenuRowHeight + generated_ui::kDisplayMenuRowGap) + expansion(s);
    return total > kViewportHeight ? total - kViewportHeight : 0;
  }

  inline uint16_t rgb565(uint32_t value) {
    return static_cast<uint16_t>(((value >> 19) & 31) << 11 | ((value >> 10) & 63) << 5 | ((value >> 3) & 31));
  }

  inline uint16_t textColor(const State& s) {
    return rgb565(generated_ui::kDisplayMenuText[s.dark ? 0 : 1]);
  }

  inline uint16_t disabledColor(const State& s) {
    return rgb565(generated_ui::kDisplayMenuDisabled[s.dark ? 0 : 1]);
  }

  inline uint16_t mutedColor(const State& s) {
    return rgb565(generated_ui::kDisplayMenuMuted[s.dark ? 0 : 1]);
  }

  inline uint16_t accentColor(const State& s) {
    return rgb565(s.dark ? generated_ui::kThemeAccentBright[clamp(s.accent, 0, 6)] : generated_ui::kThemeAccent[clamp(s.accent, 0, 6)]);
  }

  inline void begin(SduiScreen& screen, const char* id) {
    // Reset the active model in place; avoid a full-screen temporary on the ESP32 stack.
    screen.background = 0;
    screen.componentCount = 0;
    copy(screen.screenId, sizeof(screen.screenId), id);
  }

  inline void text(SduiScreen& screen, const char* id, const char* value, int x, int y, int size, uint16_t color) {
    if (screen.componentCount >= kMaxSduiComponents) return;
    auto& c = screen.components[screen.componentCount++];
    c = SduiComponent {};
    c.type = ComponentType::kText;
    copy(c.id, sizeof(c.id), id);
    copy(c.text, sizeof(c.text), value);
    c.x = x;
    c.y = y;
    c.size = size;
    c.color = color;
  }

  inline void button(SduiScreen& screen, const char* id, const char* label, int x, int y, int w, int h, uint16_t color, const char* icon = "") {
    if (screen.componentCount >= kMaxSduiComponents) return;
    auto& c = screen.components[screen.componentCount++];
    c = SduiComponent {};
    c.type = ComponentType::kButton;
    copy(c.id, sizeof(c.id), id);
    copy(c.text, sizeof(c.text), label);
    copy(c.icon, sizeof(c.icon), icon);
    c.x = x;
    c.y = y;
    c.width = w;
    c.height = h;
    c.color = color;
  }

  inline void header(SduiScreen& screen, const State& s, bool clock = false) {
    text(screen, "submenu-title", clock ? (s.spanish ? "RELOJ" : "CLOCK") : (s.spanish ? "PANTALLA" : "DISPLAY"), 196, 91, 2, textColor(s));
    button(screen, "menu-back", "‹", 48, 87, 48, 46, textColor(s));
  }

  inline void build(SduiScreen& screen, const State& s) {
    begin(screen, "device-additional-options");
    header(screen, s);
    const auto accent = accentColor(s), disabled = disabledColor(s);
    int y = kViewportY - clamp(s.scroll, 0, maxScroll(s));
    for (int row = 0; row < 6; ++row) {
      char id[40];
      snprintf(id, sizeof(id), "display-row-%s", kRows[row].key);
      button(screen, id, s.spanish ? kRows[row].spanish : kRows[row].english, kViewportX, y, kViewportWidth,
      generated_ui::kDisplayMenuRowHeight, s.expanded == row ? accent : textColor(s), s.expanded == row ? "expanded" : "row");
      y += generated_ui::kDisplayMenuRowHeight;
      if (row == s.expanded) {
        const int cy = y + 3;
        if (row == 0) {
          const char* labels[] = {"Flower", "Chevron", "Mechanical", "B&W Circle"};
          const char* spanishLabels[] = {"Flor", "Chevron", "Mecánico", "Círculo B/N"};
          const int offset = ((s.backgroundGalleryOffset % kBackgroundCount) + kBackgroundCount) % kBackgroundCount;
          for (int i = 0; i < 3; ++i) {
            char icon[20];
            const int artIndex = (offset + i) % kBackgroundCount;
            snprintf(id, sizeof(id), "display-background-%d", artIndex);
            snprintf(icon, sizeof(icon), "bgprev-%d", artIndex);
            button(screen, id, s.spanish ? spanishLabels[artIndex] : labels[artIndex], 68 + i * 105, cy + 3, 100, 135, s.background == artIndex ? accent : disabled, icon);
          }
        }
        else if (row == 1) {
          button(screen, "display-appearance-light", s.spanish ? "Claro" : "Light", 102, cy + 4, 116, 38, s.dark ? disabled : accent);
          button(screen, "display-appearance-dark", s.spanish ? "Oscuro" : "Dark", 246, cy + 4, 116, 38, s.dark ? accent : disabled);
          for (int i = 0; i < 7; ++i) {
            const int line = i < 4 ? 0 : 1, column = line ? i - 4 : i;
            char label[32];
            snprintf(id, sizeof(id), "display-accent-%s", kAccentKeys[i]);
            snprintf(label, sizeof(label), "%s%s", s.accent == i ? "[x] " : "", kAccentLabels[i]);
            button(screen, id, label, line ? 64 + column * 108 : 64 + column * 82, cy + 47 + line * 34,
            line ? 104 : 78, 30, rgb565(generated_ui::kThemeAccent[i]));
          }
        }
        else if (row == 3 || row == 4) {
          const bool units = row == 3;
          button(screen, units ? "display-units-imperial" : "display-language-en", units ? "Imperial" : "English", 102, cy + 4, 116, 38, (units ? !s.metric : !s.spanish) ? accent : disabled);
          button(screen, units ? "display-units-metric" : "display-language-es", units ? "Metric" : "Español", 246, cy + 4, 116, 38, (units ? s.metric : s.spanish) ? accent : disabled);
        }
        else if (row == 5) {
          if (!s.country) {
            button(screen, "display-country-US", "United States", 87, cy + 4, 140, 38, accent);
            button(screen, "display-country-MX", "Mexico", 239, cy + 4, 140, 38, accent);
          }
          else {
            char label[97];
            snprintf(label, sizeof(label), "%s (%s)", s.country == 1 ? "United States" : "Mexico", s.country == 1 ? "US" : "MX");
            button(screen, "display-country", label, 90, cy, 286, 34, disabled);
            button(screen, "display-postal-field", s.postal[0] ? s.postal : "Postal code", 90, cy + 37, 286, 34, accent);
            if (s.status[0]) copy(label, sizeof(label), s.status);
            else if (s.location[0]) snprintf(label, sizeof(label), "Saved: %.70s", s.location);
            else copy(label, sizeof(label), "Enter a postal code.");
            text(screen, "display-postal-status", label, 92, cy + 72, 1, mutedColor(s));
            if (s.keypad) {
              const char* keys[] = {
                "1","2","3","4","5","6","7","8","9","DEL","0","C"
              };
              for (int i = 0; i < 12; ++i) {
                snprintf(id, sizeof(id), "postal-key-%s", i == 9 ? "delete" : i == 11 ? "clear" : keys[i]);
                button(screen, id, keys[i], 113 + (i % 3) * 92, cy + 93 + (i / 3) * 62, 56, 56, disabled);
              }
              button(screen, "display-postal-cancel", s.spanish ? "Cancelar" : "Cancel", 98, cy + 344, 126, 34, disabled);
              button(screen, "display-postal-apply", "✓", 242, cy + 344, 126, 34, rgb565(0x34C759));
            }
          }
        }
        y += expansion(s);
      }
      y += generated_ui::kDisplayMenuRowGap;
    }
  }

  inline void buildClock(SduiScreen& screen, const State& s) {
    begin(screen, "device-clock-options");
    header(screen, s, true);
    const auto color = textColor(s), accent = accentColor(s), disabled = disabledColor(s);
    text(screen, "clock-format-label", s.spanish ? "Formato horario" : "Time format", 73, 143, 1, color);
    button(screen, "clock-format-12", "12 hour", 74, 165, 137, 45, s.clock24 ? disabled : accent);
    button(screen, "clock-format-24", "24 hour", 231, 165, 137, 45, s.clock24 ? accent : disabled);
    text(screen, "clock-face-label", s.spanish ? "Carátula" : "Face Selection", 73, 215, 1, color);
    const char* names[] = {
      "Minimal", "Classic", "Gold", "Digital", "Segment"
    };
    for (int i = 0; i < 5; ++i) {
      char id[32], icon[20];
      snprintf(id,sizeof(id),"clock-face-%d",i);
      snprintf(icon,sizeof(icon),"clkface-%d",i);
      button(screen,id,names[i],74+(i<3?i:i-3)*100,230+(i<3?0:51),96,45,s.clockFace==i?accent:disabled,icon);
    }
    text(screen, "clock-background-label", s.spanish ? "Fondo del reloj" : "Clock background",73,336,1,color);
    const char* backgrounds[] = {
      "Flower","Chevron","Mechanical"
    };
    for(int i=0;i<3;++i) {
      char id[32],icon[20];
      snprintf(id,sizeof(id),"clock-background-%d",i);
      snprintf(icon,sizeof(icon),"clkbg-%d",i);
      button(screen,id,backgrounds[i],74+i*100,353,96,42,s.clockBackground==i?accent:disabled,icon);
    }
    text(screen,"clock-weather-label",s.spanish?"Mostrar clima en reloj":"Show weather on clock",73,404,1,color);
    button(screen,"clock-weather-yes",s.spanish?"Sí":"Yes",231,398,68,32,s.clockWeather?accent:disabled);
    button(screen,"clock-weather-no","No",307,398,68,32,s.clockWeather?disabled:accent);
  }

  inline bool validPostal(const State& s) {
    const size_t n=strlen(s.postal);
    if (!(s.country==1?(n==5||n==9):(s.country==2&&n==5))) return false;
    for(size_t i=0;i<n;++i)if(s.postal[i]<'0'||s.postal[i]>'9')return false;
    return true;
  }

  inline Effect act(State& s,const char* id,bool clock=false) {
    if(!id)return Effect::kNone;
    if(strcmp(id,"menu-back")==0)return Effect::kBack;
    if(!clock && strncmp(id,"display-row-",12)==0) {
      const int row=rowIndex(id+12);
      if(row<0)return Effect::kNone;
      if(row==2)return Effect::kClock;
      s.expanded=s.expanded==row?-1:row;
      s.keypad=false;
      s.scroll=row==3||row==4?maxScroll(s):clamp(s.scroll,0,maxScroll(s));
      return Effect::kNone;
    }
    if(!clock && strncmp(id,"display-background-",19)==0 && strlen(id)==20 && id[19]>='0'&&id[19]<'0'+kBackgroundCount)s.background=id[19]-'0';
    else if(strcmp(id,"display-appearance-light")==0)s.dark=false;
    else if(strcmp(id,"display-appearance-dark")==0)s.dark=true;
    else if(strncmp(id,"display-accent-",15)==0) {
      bool found=false;
      for(int i=0;i<7;++i)if(strcmp(id+15,kAccentKeys[i])==0) {
        s.accent=i;
        found=true;
      }
      if(!found)return Effect::kNone;
    }
    else if(strcmp(id,"display-units-imperial")==0)s.metric=false;
    else if(strcmp(id,"display-units-metric")==0)s.metric=true;
    else if(strcmp(id,"display-language-en")==0)s.spanish=false;
    else if(strcmp(id,"display-language-es")==0)s.spanish=true;
    else if(strcmp(id,"display-country-US")==0||strcmp(id,"display-country-MX")==0) {
      s.country=strcmp(id,"display-country-US")==0?1:2;
      s.keypad=false;
      s.postal[0]=s.status[0]=0;
      s.scroll=maxScroll(s);
      return Effect::kNone;
    }
    else if(strcmp(id,"display-country")==0) {
      s.country=0;
      s.keypad=false;
      s.postal[0]=s.status[0]=0;
      s.scroll=clamp(s.scroll,0,maxScroll(s));
      return Effect::kNone;
    }
    else if(strcmp(id,"display-postal-field")==0) {
      s.keypad=true;
      s.scroll=maxScroll(s);
      return Effect::kNone;
    }
    else if(strncmp(id,"postal-key-",11)==0) {
      const char* key=id+11;
      size_t n=strlen(s.postal);
      if(strcmp(key,"delete")==0) {
        if(n)s.postal[n-1]=0;
      }
      else if(strcmp(key,"clear")==0)s.postal[0]=0;
      else if(strlen(key)==1&&key[0]>='0'&&key[0]<='9'&&n<static_cast<size_t>(s.country==1?9:5)) {
        s.postal[n]=key[0];
        s.postal[n+1]=0;
      }
      return Effect::kNone;
    }
    else if(strcmp(id,"display-postal-cancel")==0) {
      s.postal[0]=s.status[0]=0;
      s.keypad=false;
      s.scroll=maxScroll(s);
      return Effect::kNone;
    }
    else if(strcmp(id,"display-postal-apply")==0) {
      if(!validPostal(s)) {
        copy(s.status,sizeof(s.status),s.spanish?"Codigo postal no valido.":"Postal code is not valid.");
        return Effect::kNone;
      }
      return Effect::kLookup;
    }
    else if(clock && strcmp(id,"clock-format-12")==0)s.clock24=false;
    else if(clock && strcmp(id,"clock-format-24")==0)s.clock24=true;
    else if(clock && strncmp(id,"clock-face-",11)==0&&strlen(id)==12&&id[11]>='0'&&id[11]<='4')s.clockFace=id[11]-'0';
    else if(clock && strncmp(id,"clock-background-",17)==0&&strlen(id)==18&&id[17]>='0'&&id[17]<='2')s.clockBackground=id[17]-'0';
    else if(clock && strcmp(id,"clock-weather-yes")==0)s.clockWeather=true;
    else if(clock && strcmp(id,"clock-weather-no")==0)s.clockWeather=false;
    else return Effect::kNone;
    return Effect::kPreferences;
  }
  // Bounded, escaped JSON for the browser adapter and native/WASM contract tests.
  struct Json {
    char* out;
    size_t size;
    size_t used=0;
    bool ok=true;
    void raw(const char* s) {
      size_t n=strlen(s);
      if(used+n>=size) {
        ok=false;
        return;
      }
      memcpy(out+used,s,n+1);
      used+=n;
    }
    void number(int n) {
      char b[32];
      snprintf(b,sizeof(b),"%d",n);
      raw(b);
    }
    void quote(const char* s) {
      raw("\"");
      for(const unsigned char* p=reinterpret_cast<const unsigned char*>(s);*p;++p) {
        char b[8];
        if(*p=='"'||*p=='\\') {
          b[0]='\\';
          b[1]=*p;
          b[2]=0;
          raw(b);
        }
        else if(*p<32) {
          snprintf(b,sizeof(b),"\\u%04x",*p);
          raw(b);
        }
        else {
          b[0]=*p;
          b[1]=0;
          raw(b);
        }
      }
      raw("\"");
    }
  };

  inline bool json(char* out,size_t capacity,const SduiScreen& screen,const State& s,Effect effect=Effect::kNone) {
    Json j {
      out,capacity
    };
    j.raw("{\"screen\":{\"id\":");
    j.quote(screen.screenId);
    j.raw(",\"background\":\"#000000\",\"components\":[");
    for(int i=0;i<screen.componentCount;++i) {
      const auto& c=screen.components[i];
      if(i)j.raw(",");
      j.raw("{\"type\":");
      j.quote(c.type==ComponentType::kText?"text":"button");
      j.raw(",\"id\":");
      j.quote(c.id);
      j.raw(c.type==ComponentType::kText?",\"text\":":",\"label\":");
      j.quote(c.text);
      j.raw(",\"x\":");
      j.number(c.x);
      j.raw(",\"y\":");
      j.number(c.y);
      char col[8];
      snprintf(col,sizeof(col),"#%02X%02X%02X",((c.color>>11)&31)<<3,((c.color>>5)&63)<<2,(c.color&31)<<3);
      j.raw(",\"color\":");
      j.quote(col);
      if(c.type==ComponentType::kText) {
        j.raw(",\"size\":");
        j.number(c.size);
        j.raw(",\"align\":\"left\"");
      }
      else {
        j.raw(",\"width\":");
        j.number(c.width);
        j.raw(",\"height\":");
        j.number(c.height);
        j.raw(",\"icon\":");
        j.quote(c.icon);
      }
      j.raw("}");
    }
    j.raw("]},\"maxScroll\":");
    j.number(maxScroll(s));
    j.raw(",\"effect\":");
    j.number(static_cast<int>(effect));
    j.raw(",\"state\":{\"expanded\":");
    if(s.expanded>=0&&s.expanded<6)j.quote(kRows[s.expanded].key);
    else j.raw("null");
    j.raw(",\"scrollOffset\":");
    j.number(s.scroll);
    j.raw(",\"backgroundArt\":");
    j.number(s.background);
    j.raw(",\"appearanceDark\":");
    j.raw(s.dark?"true":"false");
    j.raw(",\"accentColor\":");
    j.quote(kAccentKeys[clamp(s.accent,0,6)]);
    j.raw(",\"measurementSystem\":");
    j.quote(s.metric?"metric":"imperial");
    j.raw(",\"spanishLanguage\":");
    j.raw(s.spanish?"true":"false");
    j.raw(",\"country\":");
    j.quote(s.country==1?"US":s.country==2?"MX":"");
    j.raw(",\"keypadOpen\":");
    j.raw(s.keypad?"true":"false");
    j.raw(",\"postalDraft\":");
    j.quote(s.postal);
    j.raw(",\"postalStatus\":");
    j.quote(s.status);
    j.raw(",\"clock24\":");
    j.raw(s.clock24?"true":"false");
    j.raw(",\"clockFace\":");
    j.number(s.clockFace);
    j.raw(",\"clockBackground\":");
    j.number(s.clockBackground);
    j.raw(",\"clockShowWeather\":");
    j.raw(s.clockWeather?"true":"false");
    j.raw("}}");
    if(!j.ok&&capacity)out[0]=0;
    return j.ok;
  }
}
// namespace leopold::display_menu
