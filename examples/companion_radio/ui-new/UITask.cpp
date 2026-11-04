#include "UITask.h"
#include "I18n.h"
#include "SettingsScreen.h"
#include "SpectrumScreen.h"
#include "HwConfig.h"

#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_WIFI_INTERFACE
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS 15000
#endif

#define BOOT_SCREEN_MILLIS 3000

#ifdef PIN_STATUS_LED
  #define LED_ON_MILLIS     20
  #define LED_ON_MSG_MILLIS 200
  #define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS 1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK || UI_HAS_ROTARY_INPUT
  #define UI_PRESS_TEXT(action) TR(action " press Enter")
#else
  #define UI_PRESS_TEXT(action) TR(action " long press")
#endif

#include "icons.h"

// --------------------------------------------------
// Spectrum RSSI olvasó
// --------------------------------------------------

static float readSpectrumRSSI(void* context) {
  (void)context;
  return radio_driver.getCurrentRSSI();
}

// --------------------------------------------------
// Splash képernyő
// --------------------------------------------------

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[48];


public:
  explicit SplashScreen(UITask* task) : _task(task) {

    snprintf(
      _version_info,
      sizeof(_version_info),
      "HC %s  MC %s",
      HELLCORE_VERSION,
      FIRMWARE_VERSION
    );

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    display.setColor(UIColor::corp_blue);

//    const int logoWidth = 128;
//    display.drawXbm(
//      (display.width() - logoWidth) / 2,
//      2,
//      hellcore_logo,
//      logoWidth,
//      48
//    );
const int logoWidth = 128;
const int logoHeight = 48;
//#define hellcore_logo_width 128
//#define hellcore_logo_height 48

display.drawXbm(
  (display.width() - logoWidth) / 2,
  2,
  hellcore_logo,
  logoWidth,
  logoHeight
);
    display.setColor(UIColor::primary_txt);
    display.setTextSize(1);
    display.drawTextCentered(display.width() / 2, 55, _version_info);

    display.setColor(UIColor::secondary_txt);
    display.setTextSize(1);
    display.drawTextCentered(display.width() / 2, 67, FIRMWARE_BUILD_DATE);
    //////////////////////
  //  const char* website = "https://meshcore.io";

//    display.setColor(UIColor::primary_txt);
//    display.setTextSize(1);

//    uint16_t websiteWidth = display.getTextWidth(website);
//    display.setCursor((display.width() - websiteWidth) / 2, 22);
//    display.print(website);

//    display.setColor(UIColor::primary_txt);
//    display.setTextSize(1);
//    display.drawTextCentered(display.width() / 2, 35, _version_info);

//    display.setColor(UIColor::secondary_txt);
//    display.setTextSize(1);
//    display.drawTextCentered(display.width() / 2, 48, FIRMWARE_BUILD_DATE);

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

// --------------------------------------------------
// Főképernyő
// --------------------------------------------------

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    RECENT,
    SPECTRUM,
    RADIO,
    BLUETOOTH,
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
#if UI_DISCOVER_SCREEN
    DISCOVERY,
#endif
    SETTINGS,
#ifndef UI_NO_HIBERNATE
    SHUTDOWN,
#endif
    Count
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;

  uint8_t _page;
  bool _shutdown_init;

  AdvertPath recent[UI_RECENT_LIST_SIZE];

#if UI_DISCOVER_SCREEN
  DiscoveredNode discovered[DISCOVERED_NODES_TABLE_SIZE];
  uint32_t disc_node_req_tag = 0;
  uint32_t disc_nodes_count = 0;
  uint32_t discovery_req_time = 0;
  bool discovery_disp_names = true;
#endif

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  uint32_t next_sensors_refresh = 0;

  // ------------------------------------------------
  // Akkumulátor kijelzése
  // ------------------------------------------------

  void renderBatteryIndicator(
    DisplayDriver& display,
    uint16_t batteryMilliVolts
  ) {
#ifndef BATT_MIN_MILLIVOLTS
    #define BATT_MIN_MILLIVOLTS 3000
#endif

#ifndef BATT_MAX_MILLIVOLTS
    #define BATT_MAX_MILLIVOLTS 4200
#endif

    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;

    int batteryPercentage =
      ((batteryMilliVolts - minMilliVolts) * 100) /
      (maxMilliVolts - minMilliVolts);

    if (batteryPercentage < 0) batteryPercentage = 0;
    if (batteryPercentage > 100) batteryPercentage = 100;

    const int iconWidth = 24;
    const int iconHeight = 10;
    const int iconX = display.width() - iconWidth - 5;
    const int iconY = 0;

    display.setColor(UIColor::title_txt);

    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    display.fillRect(
      iconX + iconWidth,
      iconY + (iconHeight / 4),
      3,
      iconHeight / 2
    );

    int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;

    display.fillRect(
      iconX + 2,
      iconY + 2,
      fillWidth,
      iconHeight - 4
    );

    bool charging = board.isExternalPowered();

    if (charging) {
      const int BATT_FULL_PCT = 95;

      const uint8_t* symbol =
        (batteryPercentage >= BATT_FULL_PCT)
          ? plug_icon
          : charging_icon;

      display.setColor(UIColor::title_txt);
      display.drawXbm(iconX - 9, iconY + 1, symbol, 8, 8);
    }

    if (hw_config.buzzerMuted()) {
      display.setColor(UIColor::warning_txt);

      display.drawXbm(
        iconX - (charging ? 18 : 9),
        iconY + 1,
        muted_icon,
        8,
        8
      );
    }
  }

  // ------------------------------------------------
  // Szenzoradatok frissítése
  // ------------------------------------------------

  void refresh_sensors() {
    if (_sensors == NULL) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_scroll = false;
      sensors_scroll_offset = 0;
      return;
    }

    if ((int32_t)(millis() - next_sensors_refresh) < 0) {
      return;
    }

    sensors_lpp.reset();
    sensors_nb = 0;

    sensors_lpp.addVoltage(
      TELEM_CHANNEL_SELF,
      (float)board.getBattMilliVolts() / 1000.0f
    );

    // A HomeScreen saját SensorManager példányát használjuk.
    _sensors->querySensors(0xFF, sensors_lpp);

    LPPReader reader(
      sensors_lpp.getBuffer(),
      sensors_lpp.getSize()
    );

    uint8_t channel;
    uint8_t type;

    while (reader.readHeader(channel, type)) {
      reader.skipData(type);
      sensors_nb++;
    }

    sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;

    if (sensors_nb == 0) {
      sensors_scroll_offset = 0;
    } else if (sensors_scroll_offset >= sensors_nb) {
      sensors_scroll_offset = 0;
    }

#if AUTO_OFF_MILLIS > 0
    next_sensors_refresh = millis() + 5000;
#else
    next_sensors_refresh = millis() + 60000;
#endif
  }

public:
  HomeScreen(
    UITask* task,
    mesh::RTCClock* rtc,
    SensorManager* sensors,
    NodePrefs* node_prefs
  )
    : _task(task),
      _rtc(rtc),
      _sensors(sensors),
      _node_prefs(node_prefs),
      _page(0),
      _shutdown_init(false),
      sensors_lpp(200) {
  }

#if UI_DISCOVER_SCREEN

  bool sendDiscoverRequest() {
    uint8_t cmd_bytes[6];

    cmd_bytes[0] = CTL_TYPE_NODE_DISCOVER_REQ | 1;
    cmd_bytes[1] = 0xFF;

    the_mesh.getRNG()->random(
      (uint8_t*)&disc_node_req_tag,
      4
    );

    memcpy(&cmd_bytes[2], &disc_node_req_tag, 4);

    disc_nodes_count = 0;

    mesh::Packet* req =
      the_mesh.createControlData(cmd_bytes, sizeof(cmd_bytes));

    if (req) {
      the_mesh.sendZeroHop(req);
      discovery_req_time = millis();
      return true;
    }

    return false;
  }

  void handleDiscoverResponse(const mesh::Packet* packet) {
    if (
      disc_nodes_count < DISCOVERED_NODES_TABLE_SIZE &&
      memcmp(&packet->payload[2], &disc_node_req_tag, 4) == 0
    ) {
      auto d = &discovered[disc_nodes_count++];

      memcpy(d->pubkey_prefix, &packet->payload[6], 8);

      d->type = packet->payload[0] & 0xF;
      d->snr_out = ((int8_t)packet->payload[1]) / 4.0;
      d->snr_in = radio_driver.getLastSNR();

      ContactInfo* c =
        the_mesh.lookupContactByPubKey(&packet->payload[6], 8);

      if (c != NULL) {
        strncpy(d->name, c->name, sizeof(d->name) - 1);
        d->name[sizeof(d->name) - 1] = 0;
      } else {
        d->name[0] = 0;
      }
    }
  }

#endif

  // ------------------------------------------------
  // Képernyő poll
  // ------------------------------------------------

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {
      _task->shutdown();
    }
  }

  // ------------------------------------------------
  // Képernyő render
  // ------------------------------------------------

  int render(DisplayDriver& display) override {
    display.setColor(UIColor::title_bkg);
    display.fillRect(0, 0, display.width(), 12);

    char tmp[80];

    display.setTextSize(1);
    display.setColor(UIColor::title_txt);

    char filtered_name[sizeof(_node_prefs->node_name)];

    display.translateUTF8ToBlocks(
      filtered_name,
      _node_prefs->node_name,
      sizeof(filtered_name)
    );

    display.setCursor(0, 2);
    display.print(filtered_name);

    renderBatteryIndicator(
      display,
      _task->getBattMilliVolts()
    );

    // Oldaljelző
    if (UIColor::title_bkg == UIColor::window_bkg) {
      display.setColor(UIColor::title_txt);
    } else {
      display.setColor(UIColor::title_bkg);
    }

    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count - 1);

    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x - 1, y - 1, 4, 4);
      } else {
        display.fillRect(x, y, 2, 2);
      }
    }

    // ----------------------------------------------
    // FIRST
    // ----------------------------------------------

    if (_page == HomePage::FIRST) {
      display.setColor(UIColor::primary_txt);

#ifdef ENABLE_WIFI_INTERFACE
      display.setTextSize(1);
      sprintf(tmp, TR("Unread Msg: %d"), _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 22, tmp);
#else
      display.setTextSize(2);
      sprintf(tmp, TR("MSG: %d"), _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 22, tmp);
#endif

#ifdef UI_SHOW_CLOCK

      // ----------------------------------------------
      // DIGITÁLIS ÓRA – 24 órás formátum
      // ----------------------------------------------

      uint32_t now = _rtc->getCurrentTime();
      now += (int32_t)_node_prefs->tz_offset * 3600;

      DateTime dt(now);

      // Az óra betűmérete megegyezik a Settings menüével.
      display.setTextSize(2);
      display.setColor(UIColor::primary_txt);

      // HH:MM:SS – 24 órás megjelenítés.
      snprintf(
        tmp,
        sizeof(tmp),
        "%02d:%02d:%02d",
        dt.hour(),
        dt.minute(),
        dt.second()
      );

      display.drawTextCentered(
        display.width() / 2,
        54,
        tmp
      );

      // ----------------------------------------------
      // DÁTUM – nyelvfüggő formátum
      // ----------------------------------------------

      static const char* const MONTHS[] = {
        TR("Jan"),
        TR("Feb"),
        TR("Mar"),
        TR("Apr"),
        TR("May"),
        TR("Jun"),
        TR("Jul"),
        TR("Aug"),
        TR("Sep"),
        TR("Oct"),
        TR("Nov"),
        TR("Dec")
      };

      const char* month = MONTHS[dt.month() - 1];

      switch (I18n::lang()) {

        case Lang::HU:
          // Magyar: 2026. nov. 3.
          snprintf(
            tmp,
            sizeof(tmp),
            "%04d. %s. %d.",
            dt.year(),
            month,
            dt.day()
          );
          break;

        case Lang::DE:
          // Német: 03. Nov. 2026
          snprintf(
            tmp,
            sizeof(tmp),
            "%02d. %s %04d",
            dt.day(),
            month,
            dt.year()
          );
          break;

        case Lang::EN:
        default:
          // Angol: 03 Nov 2026
          snprintf(
            tmp,
            sizeof(tmp),
            "%02d %s %04d",
            dt.day(),
            month,
            dt.year()
          );
          break;
      }

      display.setTextSize(1);
      display.setColor(UIColor::secondary_txt);

      display.drawTextCentered(
        display.width() / 2,
        68,
        tmp
      );

#endif

#ifdef ENABLE_WIFI_INTERFACE
      IPAddress ip = WiFi.localIP();

      snprintf(
        tmp,
        sizeof(tmp),
        "IP: %d.%d.%d.%d",
        ip[0], ip[1], ip[2], ip[3]
      );

      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 32, tmp);
#endif

      if (_task->hasConnection()) {
        display.setColor(UIColor::warning_txt);
        display.setTextSize(1);

#ifdef UI_SHOW_CLOCK
        display.drawTextCentered(
          display.width() / 2,
          110,
          TR("< Connected >")
        );
#else
        display.drawTextCentered(
          display.width() / 2,
          43,
          TR("< Connected >")
        );
#endif

      } else if (the_mesh.getBLEPin() != 0) {
        display.setColor(UIColor::warning_txt);

        sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());

#ifdef UI_SHOW_CLOCK
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 110, tmp);
#else
        display.setTextSize(2);
        display.drawTextCentered(display.width() / 2, 43, tmp);
#endif
      }

    // ----------------------------------------------
    // RECENT
    // ----------------------------------------------

    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);

      display.setColor(UIColor::primary_txt);

      int y = 20;

      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];

        if (a->name[0] == 0) {
          continue;
        }

        int secs = _rtc->getCurrentTime() - a->recv_timestamp;

        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60 * 60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60 * 60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];

        display.translateUTF8ToBlocks(
          filtered_recent_name,
          a->name,
          sizeof(filtered_recent_name)
        );

        display.drawTextEllipsized(
          0,
          y,
          max_name_width,
          filtered_recent_name
        );

        display.setCursor(
          display.width() - timestamp_width - 1,
          y
        );

        display.print(tmp);
      }

    // ----------------------------------------------
    // SPECTRUM
    // ----------------------------------------------

    } else if (_page == HomePage::SPECTRUM) {
      display.setColor(UIColor::corp_blue);
      display.setTextSize(2);

      display.drawTextCentered(
        display.width() / 2,
        24,
        TR("Spectrum")
      );

      display.setTextSize(1);
      display.setColor(UIColor::secondary_txt);

      display.drawTextCentered(
        display.width() / 2,
        64 - 11,
        UI_PRESS_TEXT("open:")
      );

    // ----------------------------------------------
    // RADIO
    // ----------------------------------------------

    } else if (_page == HomePage::RADIO) {
      display.setColor(UIColor::primary_txt);
      display.setTextSize(1);

      display.setCursor(0, 20);

      sprintf(
        tmp,
        "FQ: %06.3f   SF: %d",
        _node_prefs->freq,
        _node_prefs->sf
      );

      display.print(tmp);

      display.setCursor(0, 31);

      sprintf(
        tmp,
        "BW: %03.2f     CR: %d",
        _node_prefs->bw,
        _node_prefs->cr
      );

      display.print(tmp);

      display.setCursor(0, 42);

      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);

      display.setCursor(0, 53);

      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);

    // ----------------------------------------------
    // BLUETOOTH
    // ----------------------------------------------

    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(UIColor::corp_blue);

      display.drawXbm(
        (display.width() - 32) / 2,
        18,
        _task->isBluetoothEnabled() ? bluetooth_on : bluetooth_off,
        32,
        32
      );

      display.setColor(UIColor::secondary_txt);
      display.setTextSize(1);

      display.drawTextCentered(
        display.width() / 2,
        64 - 11,
        UI_PRESS_TEXT("toggle:")
      );

    // ----------------------------------------------
    // ADVERT
    // ----------------------------------------------

    } else if (_page == HomePage::ADVERT) {
      display.setColor(UIColor::corp_blue);

      display.drawXbm(
        (display.width() - 32) / 2,
        18,
        advert_icon,
        32,
        32
      );

      display.setColor(UIColor::secondary_txt);

      display.drawTextCentered(
        display.width() / 2,
        64 - 11,
        UI_PRESS_TEXT("advert:")
      );

    // ----------------------------------------------
    // GPS
    // ----------------------------------------------

#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea =
        (_sensors != NULL)
          ? _sensors->getLocationProvider()
          : NULL;

      char buf[50];
      int y = 18;

      bool gps_state = _task->getGPSState();

#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);

      if (gps_state != hw_gps_state) {
        strcpy(
          buf,
          gps_state
            ? TR("GPS off (hardware)")
            : TR("GPS off (software)")
        );
      } else {
        strcpy(buf, gps_state ? TR("GPS on") : TR("GPS off"));
      }
#else
      strcpy(buf, gps_state ? TR("GPS on") : TR("GPS off"));
#endif

      display.setColor(UIColor::primary_txt);
      display.drawTextLeftAlign(0, y, buf);

      if (nmea == NULL) {
        y += 12;

        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, TR("Can't access GPS"));

      } else {
        display.setColor(UIColor::primary_txt);

        strcpy(
          buf,
          nmea->isValid()
            ? TR("GPS fix")
            : TR("No GPS fix")
        );

        display.drawTextRightAlign(display.width() - 1, y, buf);

        y += 12;

        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, TR("Satellites"));

        display.setColor(UIColor::primary_txt);
        sprintf(buf, "%d", nmea->satellitesCount());

        display.drawTextRightAlign(display.width() - 1, y, buf);

        y += 12;

        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, TR("Position"));

        display.setColor(UIColor::primary_txt);

        sprintf(
          buf,
          "%.4f %.4f",
          nmea->getLatitude() / 1000000.0,
          nmea->getLongitude() / 1000000.0
        );

        display.drawTextRightAlign(display.width() - 1, y, buf);

        y += 12;

        display.setColor(UIColor::secondary_txt);
        display.drawTextLeftAlign(0, y, TR("Altitude"));

        display.setColor(UIColor::primary_txt);

        sprintf(
          buf,
          "%.2f",
          nmea->getAltitude() / 1000.0
        );

        display.drawTextRightAlign(display.width() - 1, y, buf);
      }
#endif

    // ----------------------------------------------
    // SENSORS
    // ----------------------------------------------

#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;

      refresh_sensors();

      char buf[30];
      char name[30];

      LPPReader reader(
        sensors_lpp.getBuffer(),
        sensors_lpp.getSize()
      );

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel;
        uint8_t type;

        if (!reader.readHeader(channel, type)) {
          break;
        }

        reader.skipData(type);
      }

      int visible_sensors =
        sensors_scroll
          ? UI_RECENT_LIST_SIZE
          : sensors_nb;

      for (int i = 0; i < visible_sensors; i++) {
        uint8_t channel;
        uint8_t type;

        if (!reader.readHeader(channel, type)) {
          break;
        }

        display.setCursor(0, y);

        float v;

        switch (type) {
          case LPP_GPS: {
            float lat;
            float lon;
            float alt;

            reader.readGPS(lat, lon, alt);

            strcpy(name, TR("GPS"));
            sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          }

          case LPP_VOLTAGE:
            reader.readVoltage(v);
            strcpy(name, TR("Voltage"));
            sprintf(buf, "%6.2f", v);
            break;

          case LPP_CURRENT:
            reader.readCurrent(v);
            strcpy(name, TR("Current"));
            sprintf(buf, "%.3f", v);
            break;

          case LPP_TEMPERATURE:
            reader.readTemperature(v);
            strcpy(name, TR("Temperature"));
            sprintf(buf, "%.2f", v);
            break;

          case LPP_RELATIVE_HUMIDITY:
            reader.readRelativeHumidity(v);
            strcpy(name, TR("Humidity"));
            sprintf(buf, "%.2f", v);
            break;

          case LPP_BAROMETRIC_PRESSURE:
            reader.readPressure(v);
            strcpy(name, TR("Pressure"));
            sprintf(buf, "%.2f", v);
            break;

          case LPP_ALTITUDE:
            reader.readAltitude(v);
            strcpy(name, TR("Altitude"));
            sprintf(buf, "%.0f", v);
            break;

          case LPP_POWER:
            reader.readPower(v);
            strcpy(name, TR("Power"));
            sprintf(buf, "%6.2f", v);
            break;

          default:
            reader.skipData(type);
            strcpy(name, TR("Unknown"));
            buf[0] = 0;
            break;
        }

        display.setCursor(0, y);
        display.setColor(UIColor::secondary_txt);
        display.print(name);

        display.setColor(UIColor::primary_txt);

        display.setCursor(
          display.width() - display.getTextWidth(buf) - 1,
          y
        );

        display.print(buf);

        y += 12;
      }

      if (sensors_scroll && sensors_nb > 0) {
        sensors_scroll_offset =
          (sensors_scroll_offset + 1) % sensors_nb;
      } else {
        sensors_scroll_offset = 0;
      }
#endif

    // ----------------------------------------------
    // DISCOVERY
    // ----------------------------------------------

#if UI_DISCOVER_SCREEN
    } else if (_page == HomePage::DISCOVERY) {
      int count = disc_nodes_count;

      display.setColor(UIColor::primary_txt);

      int y = 20;

      for (int i = 0; i < count; i++, y += 11) {
        char name[32];
        char filtered_name[sizeof(name)];
        char snr_s[12];

        auto a = &discovered[i];

        if ((a->name[0] == 0) || !discovery_disp_names) {
          mesh::Utils::toHex(name, a->pubkey_prefix, 4);
        } else {
          strncpy(name, a->name, sizeof(name) - 1);
          name[sizeof(name) - 1] = 0;
        }

        if (strlen(name) <= 8) {
          sprintf(snr_s, "%02.1f>%02.1f", a->snr_out, a->snr_in);
        } else {
          sprintf(snr_s, "%02.1f", a->snr_in);
        }

        int snr_width = display.getTextWidth(snr_s);
        int max_name_width = display.width() - snr_width - 1;

        display.translateUTF8ToBlocks(
          filtered_name,
          name,
          sizeof(filtered_name)
        );

        display.drawTextEllipsized(
          0,
          y,
          max_name_width,
          filtered_name
        );

        display.setCursor(
          display.width() - snr_width - 1,
          y
        );

        display.print(snr_s);
      }

      if (millis() < discovery_req_time + 5000) {
        return 1000;
      } else if (count < DISCOVERED_NODES_TABLE_SIZE - 1) {
        y = 10 + 11 * DISCOVERED_NODES_TABLE_SIZE;

        display.drawTextCentered(
          display.width() / 2,
          y,
          UI_PRESS_TEXT("discover:")
        );
      }
#endif

    // ----------------------------------------------
    // SETTINGS
    // ----------------------------------------------

    } else if (_page == HomePage::SETTINGS) {
      display.setColor(UIColor::corp_blue);
      display.setTextSize(2);

      display.drawTextCentered(
        display.width() / 2,
        24,
        TR("Settings")
      );

      display.setTextSize(1);
      display.setColor(UIColor::secondary_txt);

      display.drawTextCentered(
        display.width() / 2,
        64 - 11,
        UI_PRESS_TEXT("open:")
      );

    // ----------------------------------------------
    // SHUTDOWN
    // ----------------------------------------------

#ifndef UI_NO_HIBERNATE
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(UIColor::corp_blue);
      display.setTextSize(1);

      if (_shutdown_init) {
        display.setColor(UIColor::warning_txt);

        display.drawTextCentered(
          display.width() / 2,
          34,
          TR("Hibernating...")
        );

      } else {
        display.setColor(UIColor::secondary_txt);

        display.drawXbm(
          (display.width() - 32) / 2,
          18,
          power_icon,
          32,
          32
        );

        display.drawTextCentered(
          display.width() / 2,
          64 - 11,
          UI_PRESS_TEXT("hibernate:")
        );
      }
#endif
    }

    return 5000;
  }

  // ------------------------------------------------
  // Bemenetkezelés
  // ------------------------------------------------

  bool handleInput(char c) override {
    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      return true;
    }

    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = (_page + 1) % HomePage::Count;

      if (_page == HomePage::RECENT) {
        _task->showAlert(TR("Recent adverts"), 800);
      }

#if UI_DISCOVER_SCREEN
      if (_page == HomePage::DISCOVERY) {
        _task->showAlert(TR("Repeater discovery"), 800);
      }
#endif

      return true;
    }

    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isBluetoothEnabled()) {
        _task->disableBluetooth();
      } else {
        _task->enableBluetooth();
      }

      return true;
    }

    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);

      if (the_mesh.advert()) {
        _task->showAlert(TR("Advert sent!"), 1000);
      } else {
        _task->showAlert(TR("Advert failed.."), 1000);
      }

      return true;
    }

    if (c == KEY_ENTER && _page == HomePage::SPECTRUM) {
      _task->gotoSpectrumScreen();
      return true;
    }

    if (c == KEY_ENTER && _page == HomePage::SETTINGS) {
      _task->gotoSettingsScreen();
      return true;
    }

#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif

    // A SENSORS oldal Enter gombja nem kapcsolhatja a GPS-t.
    // A szenzoradatok frissítését a refresh_sensors() időzítése végzi.
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      next_sensors_refresh = 0;
      sensors_scroll_offset = 0;
      return true;
    }
#endif

#if UI_DISCOVER_SCREEN
    if (c == KEY_ENTER && _page == HomePage::DISCOVERY) {
      if (millis() > discovery_req_time + 5000) {
        sendDiscoverRequest();
      }

      return true;
    }

    if (c == KEY_SELECT && _page == HomePage::DISCOVERY) {
      discovery_disp_names = !discovery_disp_names;
      return true;
    }
#endif

#ifndef UI_NO_HIBERNATE
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;
      return true;
    }
#endif

    return false;
  }
};

// --------------------------------------------------
// Üzenet-előnézet
// --------------------------------------------------

#ifndef UI_MSG_PREVIEW_SIZE
  #define UI_MSG_PREVIEW_SIZE 78
#endif

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    char origin[62];
    char msg[UI_MSG_PREVIEW_SIZE];
  };

#define MAX_UNREAD_MSGS 32

  int num_unread;
  int head = MAX_UNREAD_MSGS - 1;
  MsgEntry unread[MAX_UNREAD_MSGS];

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc)
    : _task(task), _rtc(rtc) {
    num_unread = 0;
  }

  void addPreview(
    uint8_t path_len,
    const char* from_name,
    const char* msg
  ) {
    head = (head + 1) % MAX_UNREAD_MSGS;

    if (num_unread < MAX_UNREAD_MSGS) {
      num_unread++;
    }

    auto p = &unread[head];

    p->timestamp = _rtc->getCurrentTime();

    if (path_len == 0xFF) {
      snprintf(
        p->origin,
        sizeof(p->origin),
        "(D) %s:",
        from_name
      );
    } else {
      snprintf(
        p->origin,
        sizeof(p->origin),
        "(%d) %s:",
        (uint32_t)path_len,
        from_name
      );
    }

    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
  }

  int render(DisplayDriver& display) override {
    char tmp[16];

    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(UIColor::corp_blue);

    sprintf(tmp, TR("Unread: %d"), num_unread);
    display.print(tmp);

    auto p = &unread[head];

    int secs = _rtc->getCurrentTime() - p->timestamp;

    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60 * 60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60 * 60));
    }

    display.setCursor(
      display.width() - display.getTextWidth(tmp) - 2,
      0
    );

    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);

    display.setCursor(0, 14);
    display.setColor(UIColor::secondary_txt);

    char filtered_origin[sizeof(p->origin)];

    display.translateUTF8ToBlocks(
      filtered_origin,
      p->origin,
      sizeof(filtered_origin)
    );

    display.print(filtered_origin);

    display.setCursor(0, 25);
    display.setColor(UIColor::primary_txt);

    char filtered_msg[sizeof(p->msg)];

    display.translateUTF8ToBlocks(
      filtered_msg,
      p->msg,
      sizeof(filtered_msg)
    );

    display.printWordWrap(filtered_msg, display.width());

#if AUTO_OFF_MILLIS == 0
    return 10000;
#else
    return 1000;
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      if (num_unread > 0) {
        head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
        num_unread--;
      }

      if (num_unread == 0) {
        _task->gotoHomeScreen();
      }

      return true;
    }

    if (c == KEY_ENTER) {
      num_unread = 0;
      _task->gotoHomeScreen();
      return true;
    }

    return false;
  }
};

// --------------------------------------------------
// Beállítások backend
// --------------------------------------------------

class UISettingsBackend : public SettingsBackend {
  UITask* _t;

  static const char* onOff(bool value) {
    return value ? TR("On") : TR("Off");
  }

public:
  explicit UISettingsBackend(UITask* task)
    : _t(task) {
  }

  void getValue(
    SettingsItem item,
    char* out,
    size_t n
  ) override {
    if (n == 0) {
      return;
    }

    out[0] = 0;

    switch (item) {
      case SettingsItem::LANGUAGE:
        snprintf(
          out,
          n,
          "%s",
          I18n::langName(I18n::lang())
        );
        break;

      case SettingsItem::SCREEN_TIMEOUT:
        hw_config.timeoutLabel(
          hw_config.timeoutIdx(),
          out,
          n
        );
        break;

      case SettingsItem::KEEP_USB:
        snprintf(out, n, "%s", onOff(_t->_keep_usb_on));
        break;

      case SettingsItem::MSG_WAKE:
        snprintf(out, n, "%s", onOff(_t->_msg_wake));
        break;

      case SettingsItem::BLUETOOTH:
        snprintf(
          out,
          n,
          "%s",
          onOff(_t->isBluetoothEnabled())
        );
        break;

      case SettingsItem::GPS:
        snprintf(
          out,
          n,
          "%s",
          onOff(_t->getGPSState())
        );
        break;

      case SettingsItem::BUZZER:
        if (!hw_config.sensorEnabled(HwSensor::Buzzer)) {
          snprintf(out, n, "%s", TR("Off"));
        } else if (hw_config.pin(HwPin::Buzzer) < 0) {
          snprintf(out, n, "%s", TR("No pin"));
        } else {
          snprintf(out, n, "%s", TR("On"));
        }
        break;

      case SettingsItem::VIBRA:
        if (!hw_config.sensorEnabled(HwSensor::Vibra)) {
          snprintf(out, n, "%s", TR("Off"));
        } else if (hw_config.pin(HwPin::Vibra) < 0) {
          snprintf(out, n, "%s", TR("No pin"));
        } else {
          snprintf(out, n, "%s", TR("On"));
        }
        break;

      // A BRIGHTNESS beállítás a SettingsScreen-ben szerepel,
      // de a jelenlegi DisplayDriver interfészhez nincs itt
      // igazolt fényerőállító metódus.
      case SettingsItem::BRIGHTNESS:
        snprintf(
          out,
          n,
          "%u%%",
          (unsigned int)hw_config.brightness()
        );
        break;

      default:
        break;
    }
  }

  void change(SettingsItem item, int8_t dir) override {
    switch (item) {
      case SettingsItem::LANGUAGE: {
        int index = (int)I18n::lang();

        if (dir < 0) {
          index =
            (index + (int)Lang::COUNT - 1) %
            (int)Lang::COUNT;
        } else {
          index =
            (index + 1) %
            (int)Lang::COUNT;
        }

        I18n::setLang((Lang)index);
        I18n::save();
        break;
      }

      case SettingsItem::SCREEN_TIMEOUT: {
        uint8_t count = hw_config.timeoutCount();
        uint8_t idx = hw_config.timeoutIdx();

        if (count == 0) {
          break;
        }

        idx = (dir < 0)
          ? (idx + count - 1) % count
          : (idx + 1) % count;

        hw_config.setTimeoutIdx(idx);

        _t->_auto_off_ms = hw_config.timeoutMillis(idx);
        _t->_auto_off = millis() + _t->_auto_off_ms;
        break;
      }

      case SettingsItem::KEEP_USB:
        _t->_keep_usb_on = !_t->_keep_usb_on;
        hw_config.setKeepUsb(_t->_keep_usb_on);
        break;

      case SettingsItem::MSG_WAKE:
        _t->_msg_wake = !_t->_msg_wake;
        hw_config.setMsgWake(_t->_msg_wake);
        break;

      case SettingsItem::BLUETOOTH:
        if (_t->isBluetoothEnabled()) {
          _t->disableBluetooth();
        } else {
          _t->enableBluetooth();
        }
        break;

      case SettingsItem::GPS:
        _t->toggleGPS();
        break;

      case SettingsItem::BUZZER: {
        bool on = !hw_config.sensorEnabled(HwSensor::Buzzer);

        hw_config.setSensor(HwSensor::Buzzer, on);

        if (on) {
          hw_config.testBuzzer();
        }

        break;
      }

      case SettingsItem::VIBRA: {
        bool on = !hw_config.sensorEnabled(HwSensor::Vibra);

        hw_config.setSensor(HwSensor::Vibra, on);

        if (on) {
          hw_config.testVibra();
        }

        break;
      }

      case SettingsItem::FLOOD_ADVERT:
        // Hirdetés kizárólag Enter/OK műveletre.
        // A balra/jobbra gomb nem indíthat advertet.
        if (dir == 0) {
          _t->notify(UIEventType::ack);

          if (the_mesh.advert()) {
            _t->showAlert(TR("Advert sent!"), 1000);
          } else {
            _t->showAlert(TR("Advert failed.."), 1000);
          }
        }
        break;

      case SettingsItem::REBOOT:
        if (dir == 0) {
          _t->shutdown(true);
        }
        break;

      case SettingsItem::POWER_OFF:
        if (dir == 0) {
          _t->shutdown(false);
        }
        break;

      case SettingsItem::BRIGHTNESS: {
        static const uint8_t brightness_levels[] = {
          25,
          50,
          75,
          100
        };

        static const uint8_t brightness_count =
          sizeof(brightness_levels) /
          sizeof(brightness_levels[0]);

        uint8_t current = hw_config.brightness();
        uint8_t index = 0;

        // Megkeressük az aktuális fényerőszintet.
        for (uint8_t i = 0; i < brightness_count; i++) {
          if (brightness_levels[i] == current) {
            index = i;
            break;
          }
        }

        // Balra: csökkentés.
        // Jobbra vagy OK: növelés.
        if (dir < 0) {
          index = (index + brightness_count - 1) %
                  brightness_count;
        } else {
          index = (index + 1) % brightness_count;
        }

        uint8_t new_brightness = brightness_levels[index];

        // Beállítás mentése a hardverkonfigurációba.
        hw_config.setBrightness(new_brightness);

        // Azonnali alkalmazás a kijelzőn.
        if (_t->_display != NULL) {
          _t->_display->setBrightness(new_brightness);
        }

        break;
      }

      case SettingsItem::SENSORS:
      case SettingsItem::PINS:
      case SettingsItem::COUNT:
      default:
        break;
    }

    _t->_next_refresh = 0;
  }

  void back() override {
    _t->gotoHomeScreen();
  }
};

// --------------------------------------------------
// UITask inicializálása
// --------------------------------------------------

void UITask::begin(
  DisplayDriver* display,
  SensorManager* sensors,
  NodePrefs* node_prefs
) {
  _display = display;
  _sensors = sensors;
  _node_prefs = node_prefs;

  // Nyelvi beállítás betöltése.
  I18n::load();

  // Hardverbeállítások betöltése.
  hw_config.begin();

  _auto_off_ms =
    hw_config.timeoutMillis(hw_config.timeoutIdx());

  _keep_usb_on = hw_config.keepUsb();
  _msg_wake = hw_config.msgWake();

  _auto_off = millis() + _auto_off_ms;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif

#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  if (_display != NULL) {
    // A mentett fényerő alkalmazása.
    // A driver kikapcsolt állapotban is eltárolja az értéket.
    _display->setBrightness(hw_config.brightness());

    // Bekapcsoláskor a driver alkalmazza a beállított fényerőt.
    _display->turnOn();
  }

  hw_config.startupBeep();

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);

  home = new HomeScreen(
    this,
    &rtc_clock,
    sensors,
    node_prefs
  );

  msg_preview = new MsgPreviewScreen(
    this,
    &rtc_clock
  );

  spectrum = new SpectrumScreen(
    this,
    readSpectrumRSSI
  );

  settings_backend = new UISettingsBackend(this);
  settings = new SettingsScreen(settings_backend);

  setCurrScreen(splash);
}

// --------------------------------------------------
// Képernyőváltások
// --------------------------------------------------

void UITask::gotoSpectrumScreen() {
  ((SpectrumScreen*)spectrum)->reset();
  setCurrScreen(spectrum);
}

void UITask::gotoSettingsScreen() {
  ((SettingsScreen*)settings)->resetSelection();
  setCurrScreen(settings);
}

// --------------------------------------------------
// Értesítés
// --------------------------------------------------

void UITask::showAlert(
  const char* text,
  int duration_millis
) {
  snprintf(_alert, sizeof(_alert), "%s", text ? text : "");
  _alert_expiry = millis() + duration_millis;
}

// --------------------------------------------------
// UI események
// --------------------------------------------------

void UITask::notify(UIEventType t) {
  switch (t) {
    case UIEventType::contactMessage:
    case UIEventType::channelMessage:
      hw_config.messageAlert();
      break;

    case UIEventType::ack:
      hw_config.ackTick();
      break;

    case UIEventType::roomMessage:
    case UIEventType::newContactMessage:
    case UIEventType::none:
    default:
      break;
  }
}

// --------------------------------------------------
// Üzenet fogadása
// --------------------------------------------------

void UITask::onMessageRecv(
  mesh::Packet* pkt,
  const ContactInfo& from,
  uint8_t txt_type,
  uint32_t sender_timestamp,
  const char* text
) {
  (void)sender_timestamp;

  if (
    !(txt_type == TXT_TYPE_PLAIN ||
      txt_type == TXT_TYPE_SIGNED_PLAIN)
  ) {
    return;
  }

  uint8_t path_len =
    pkt->isRouteFlood()
      ? pkt->path_len
      : 0xFF;

  ((MsgPreviewScreen*)msg_preview)->addPreview(
    path_len,
    from.name,
    text
  );

  setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn() && _msg_wake && !hasConnection()) {
      _display->turnOn();
    }

    if (_display->isOn()) {
      _auto_off = millis() + _auto_off_ms;
      _next_refresh = 100;
    }
  }

  if (!hasConnection()) {
    notify(UIEventType::contactMessage);
  }
}

void UITask::onChannelMessageRecv(
  mesh::Packet* pkt,
  ChannelDetails& channel_details,
  const char* text
) {
  uint8_t path_len =
    pkt->isRouteFlood()
      ? pkt->path_len
      : 0xFF;

  ((MsgPreviewScreen*)msg_preview)->addPreview(
    path_len,
    channel_details.name,
    text
  );

  setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn() && _msg_wake && !hasConnection()) {
      _display->turnOn();
    }

    if (_display->isOn()) {
      _auto_off = millis() + _auto_off_ms;
      _next_refresh = 100;
    }
  }

  if (!hasConnection()) {
    notify(UIEventType::channelMessage);
  }
}

// --------------------------------------------------
// Üzenetszám változása
// --------------------------------------------------

void UITask::onQueueSizeChanged(int msgcount) {
  _msgcount = msgcount;

  if (msgcount == 0 && curr == msg_preview) {
    gotoHomeScreen();
  }
}

// --------------------------------------------------
// Új csomópont
// --------------------------------------------------

void UITask::onDiscoveredContact(
  ContactInfo& contact,
  bool is_new,
  uint8_t path_len,
  const uint8_t* path
) {
  (void)contact;
  (void)is_new;
  (void)path_len;
  (void)path;

  if (!hasConnection()) {
    notify(UIEventType::newContactMessage);
  }
}

// --------------------------------------------------
// Discovery válasz
// --------------------------------------------------

void UITask::onControlDataRecv(const mesh::Packet* packet) {
#if UI_DISCOVER_SCREEN
  if (
    packet->payload_len >= 14 &&
    (packet->payload[0] & 0xF0) == CTL_TYPE_NODE_DISCOVER_RESP
  ) {
    ((HomeScreen*)home)->handleDiscoverResponse(packet);
  }
#endif
}

// --------------------------------------------------
// LED kezelés
// --------------------------------------------------

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();

  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;

      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }

      next_led_change = cur_time + last_led_increment;

    } else {
      led_state = 0;

      next_led_change =
        cur_time +
        LED_CYCLE_MILLIS -
        last_led_increment;
    }

    digitalWrite(
      PIN_STATUS_LED,
      led_state == LED_STATE_ON
    );
  }
#endif
}

// --------------------------------------------------
// Aktuális képernyő beállítása
// --------------------------------------------------

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

// --------------------------------------------------
// Leállítás / újraindítás
// --------------------------------------------------

void UITask::shutdown(bool restart) {
  hw_config.save();

  hw_config.shutdownBeep();

  uint32_t beep_timer = millis();

  while (
    hw_config.busy() &&
    (millis() - beep_timer) < 2500
  ) {
    hw_config.loop();
  }

  if (restart) {
    _board->reboot();

  } else {
    display.forceFullRefresh();
    display.clear();
    display.endFrame();

    _board->powerOff();
  }
}

// --------------------------------------------------
// Gomb állapota
// --------------------------------------------------

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

// --------------------------------------------------
// Fő UI ciklus
// --------------------------------------------------

void UITask::loop() {
  char c = 0;

#if UI_HAS_JOYSTICK

  int ev = user_btn.check();

  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  }

  ev = joystick_left.check();

  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }

  ev = joystick_right.check();

  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }

  ev = back_btn.check();

  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }

#elif defined(PIN_USER_BTN)

  int ev = user_btn.check();

#ifdef UI_HAS_NAV_INPUT

  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    display.turnOff();
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_SELECT);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }

#else

  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }

#endif
#endif

#if defined(UI_HAS_ROTARY_INPUT)

  RotaryInputEvent rotaryEv = rotary_input.poll();

  if (c == 0 && _display != NULL && _display->isOn()) {
    if (rotaryEv == RotaryInputEvent::Next) {
      c = KEY_NEXT;
    } else if (rotaryEv == RotaryInputEvent::Prev) {
      c = KEY_PREV;
    }
  }

#endif

#if defined(PIN_USER_BTN_ANA)

  if ((millis() - _analogue_pin_read_millis) > 10) {
    int ev = analog_btn.check();

    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }

    _analogue_pin_read_millis = millis();
  }

#endif

#if defined(BACKLIGHT_BTN)

  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);

#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif

    next_backlight_btn_check = millis() + 300;
  }

#endif

  // Bemenet továbbítása az aktuális képernyőnek.
  if (c != 0 && curr) {
    curr->handleInput(c);

    _auto_off = millis() + _auto_off_ms;
    _next_refresh = 100;
  }

  userLedHandler();

  // Hangjelzés és rezgés kezelése.
  hw_config.loop();

  if (curr) {
    curr->poll();
  }

  // ------------------------------------------------
  // Kijelző frissítése
  // ------------------------------------------------

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();

      int delay_millis = curr->render(*_display);

      if (millis() < _alert_expiry) {
        _display->setTextSize(1);

        int y = _display->height() / 3;
        int p = _display->height() / 32;

        _display->setColor(UIColor::popup_bkg);

        _display->fillRect(
          p,
          y,
          _display->width() - p * 2,
          y
        );

        _display->setColor(UIColor::popup_txt);

        _display->drawRect(
          p,
          y,
          _display->width() - p * 2,
          y
        );

        _display->drawTextCentered(
          _display->width() / 2,
          y + p * 3,
          _alert
        );

        _next_refresh = _alert_expiry;

      } else {
        _next_refresh = millis() + delay_millis;
      }

      _display->endFrame();
    }

    // USB táplálásnál bekapcsolva tartott kijelző.
    if (_keep_usb_on && board.isExternalPowered()) {
      _auto_off = millis() + _auto_off_ms;
    }

    if (_auto_off_ms > 0 && millis() > _auto_off) {
      _display->turnOff();
    }
  }

  // ------------------------------------------------
  // Automatikus leállítás alacsony akkufeszültségnél
  // ------------------------------------------------

#ifdef AUTO_SHUTDOWN_MILLIVOLTS

  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();

    if (
      milliVolts > 0 &&
      milliVolts < AUTO_SHUTDOWN_MILLIVOLTS
    ) {
      if (!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();

          _display->setTextSize(2);
          _display->setColor(UIColor::warning_txt);

          _display->drawTextCentered(
            _display->width() / 2,
            20,
            TR("Low Battery.")
          );

          _display->drawTextCentered(
            _display->width() / 2,
            40,
            TR("Shutting Down!")
          );

          _display->endFrame();

          if (_display->isEink() == false) {
            delay(3000);
          }
        }

        shutdown();
      }
    }

    next_batt_chck = millis() + 8000;
  }

#endif
}

// --------------------------------------------------
// Kijelző bekapcsolása bemenet esetén
// --------------------------------------------------

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();
      c = 0;
    }

    _auto_off = millis() + _auto_off_ms;
    _next_refresh = 0;
  }

  return c;
}

// --------------------------------------------------
// Hosszú gombnyomás
// --------------------------------------------------

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {
    the_mesh.enterCLIRescue();
    c = 0;
  }

  return c;
}

// --------------------------------------------------
// Dupla kattintás
// --------------------------------------------------

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");

  checkDisplayOn(c);

  return c;
}

// --------------------------------------------------
// Tripla kattintás
// --------------------------------------------------

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");

  checkDisplayOn(c);

  toggleBuzzer();

  c = 0;

  return c;
}

// --------------------------------------------------
// GPS állapot lekérdezése
// --------------------------------------------------

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();

    for (int i = 0; i < num; i++) {
      if (
        strcmp(
          _sensors->getSettingName(i),
          "gps"
        ) == 0
      ) {
        return !strcmp(
          _sensors->getSettingValue(i),
          "1"
        );
      }
    }
  }

  return false;
}

// --------------------------------------------------
// GPS be/ki kapcsolása
// --------------------------------------------------

void UITask::toggleGPS() {
  if (_sensors == NULL) {
    return;
  }

  int num = _sensors->getNumSettings();

  for (int i = 0; i < num; i++) {
    if (
      strcmp(
        _sensors->getSettingName(i),
        "gps"
      ) == 0
    ) {
      if (
        strcmp(
          _sensors->getSettingValue(i),
          "1"
        ) == 0
      ) {
        _sensors->setSettingValue("gps", "0");
        _node_prefs->gps_enabled = 0;
      } else {
        _sensors->setSettingValue("gps", "1");
        _node_prefs->gps_enabled = 1;
      }

      notify(UIEventType::ack);

      the_mesh.savePrefs();

      showAlert(
        _node_prefs->gps_enabled
          ? TR("GPS: Enabled")
          : TR("GPS: Disabled"),
        800
      );

      _next_refresh = 0;
      break;
    }
  }
}

// --------------------------------------------------
// Buzzer be/ki kapcsolása
// --------------------------------------------------

void UITask::toggleBuzzer() {
  bool on = !hw_config.sensorEnabled(HwSensor::Buzzer);

  hw_config.setSensor(HwSensor::Buzzer, on);

  if (on) {
    hw_config.testBuzzer();
  }

  showAlert(
    on ? TR("Buzzer: ON") : TR("Buzzer: OFF"),
    800
  );

  _next_refresh = 0;
}
