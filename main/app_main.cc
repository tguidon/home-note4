#include "battery_history.h"
#include "feeds.h"
#include "display_worker.h"
#include "network.h"
#include "render.h"
#include <ctime>
#include <cstring>
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "zectrix_board.h"
#include "zectrix_epd.h"

namespace {
class HomeNote {
public:
    void Run() {
        // GPIO17 power latch is established by the official board adapter first.
        ESP_ERROR_CHECK(board_.Init());
        zectrix_epd_config_t cfg; zectrix_epd_get_default_config(&cfg);
        ESP_ERROR_CHECK(zectrix_epd_new(&cfg, &epd_));
        ESP_ERROR_CHECK(display_.Init(epd_));
        Message("Starting up", {"Weather and your calendar", "US Open, Formula 1 and NFL", "Connecting to your home..."});
        auto err = home::InitNetwork();
        if (err != ESP_OK) {
            Message("Storage needs attention", {"Settings could not be read.", "Nothing has been erased.", "Check the USB serial log."});
            ESP_LOGE("home_note", "network init: %s", esp_err_to_name(err)); return;
        }
        const auto history_err = battery_history_.Init();
        if (history_err != ESP_OK) ESP_LOGW("home_note", "battery history init: %s", esp_err_to_name(history_err));
        boot_id_ = esp_random();
        usb_serial_jtag_driver_config_t usb_config = {1024, 256};
        const auto usb_err = usb_serial_jtag_driver_install(&usb_config);
        usb_commands_ = usb_err == ESP_OK;
        if (usb_commands_) usb_serial_jtag_vfs_use_driver();
        else ESP_LOGW("home_note", "USB battery export unavailable: %s", esp_err_to_name(usb_err));
        if (!home::HasWifiCredentials() || !home::ConnectWifi(30000)) { Setup(); return; }
        home::StartClock(); home::StartFeeds(); dashboard_ready_ = true;
        home::Dashboard data;
        unsigned revision = ~0u;
        bool online = false, redraw = true;
        time_t shown_day = 0, shown_interval = 0;
        while (true) {
            BatteryTelemetry();
            const auto tick = esp_timer_get_time();
            if (led_until_ && tick >= led_until_) { board_.SetPowerLed(false); led_until_ = 0; }
            const time_t now = time(nullptr);
            const int day = home::LocalDateKey(now);
            if (day != shown_day) detail_page_ = 0;
            if (home::FeedRevision() != revision) {
                auto next = home::Snapshot();
                if (section_ == 2 && (next.f1.latest.session.round_id != data.f1.latest.session.round_id ||
                                     next.f1.latest.session.code != data.f1.latest.session.code)) detail_page_ = 0;
                data = std::move(next); revision = data.revision; redraw = true;
            }
            if (online != home::WifiConnected() || day != shown_day || now / 300 != shown_interval) redraw = true;
            if (home::TennisEnded(data.tennis, now)) data.tennis.matches.clear();
            // Consume queued taps before drawing, so fast navigation goes directly
            // to the latest selected section instead of replaying old screens.
            ZectrixButtonEvent event;
            while (board_.WaitButton(&event, 0)) {
                Acknowledge();
                if (event.action == ZectrixButtonAction::kLongPress) {
                    if (event.button == ZectrixButton::kDown) Shutdown();
                    if (event.button == ZectrixButton::kOk) home::SetupOnNextBoot();
                } else Navigate(event.button, data);
                redraw = true;
            }
            if (navigation_requested_) { navigation_requested_ = false; redraw = true; }
            detail_page_ %= home::SectionPages(data, section_);
            if (redraw) {
                online = home::WifiConnected();
                const auto power = board_.ReadPowerSnapshot();
                const int battery_percent = power.battery_valid ? power.battery_percent : -1;
                const int page = home::SectionStart(data, section_) + detail_page_;
                home::RenderDashboard(canvas_, data, page, now, online, battery_percent);
                Display(); redraw = false;
                shown_day = day; shown_interval = now / 300;
            }
            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
private:
    void Acknowledge() {
        board_.SetPowerLed(true);
        led_until_ = esp_timer_get_time() + 120000;
    }
    void Navigate(ZectrixButton button, const home::Dashboard& data) {
        if (button == ZectrixButton::kUp || button == ZectrixButton::kDown) {
            section_ = (section_ + (button == ZectrixButton::kUp ? 3 : 1)) % 4;
            detail_page_ = 0;
        } else if (home::SectionPages(data, section_) > 1) {
            detail_page_ = (detail_page_ + 1) % home::SectionPages(data, section_);
        } else home::RefreshFeeds();
        ESP_LOGI("home_note", "navigation: section %d, detail %d", section_, detail_page_);
    }
    void RecordBattery() {
        const auto power = board_.ReadPowerSnapshot();
        home::BatteryRecord record;
        record.boot_id = boot_id_;
        record.uptime_s = esp_timer_get_time() / 1000000;
        const time_t now = time(nullptr);
        record.unix_time = now >= 1704067200 ? now : 0;
        record.millivolts = power.battery_mv;
        record.percent = power.battery_percent;
        record.flags = (power.battery_valid ? home::kBatteryValid : 0) |
            (power.charge.power_present ? home::kExternalPower : 0) |
            (power.charge.charging ? home::kCharging : 0) |
            (power.charge.full ? home::kFull : 0) |
            (power.charge.fault ? home::kFault : 0) |
            (power.charge.no_battery ? home::kNoBattery : 0) |
            (home::WifiConnected() ? home::kWifiConnected : 0);
        const auto err = battery_history_.Append(record);
        if (err != ESP_OK) ESP_LOGW("home_note", "battery history write: %s", esp_err_to_name(err));
        else ESP_LOGI("home_note", "battery history saved: %u mV, %u%%, external power=%d, charging=%d",
                      record.millivolts, record.percent, power.charge.power_present, power.charge.charging);
        next_battery_sample_ = esp_timer_get_time() + 900LL * 1000000;
    }
    void BatteryTelemetry() {
        if (esp_timer_get_time() >= next_battery_sample_) RecordBattery();
        if (!usb_commands_) return;
        char incoming[32];
        const int count = usb_serial_jtag_read_bytes(incoming, sizeof(incoming), 0);
        for (int i = 0; i < count; ++i) {
            const char c = incoming[i];
            if (c == '\n' || c == '\r') {
                if (!command_overflow_ && std::strcmp(usb_command_, "battery") == 0) {
                    const auto err = battery_history_.DumpCsv(stdout);
                    if (err != ESP_OK) ESP_LOGW("home_note", "battery export failed: %s", esp_err_to_name(err));
                }
                if (!command_overflow_ && dashboard_ready_) {
                    if (std::strcmp(usb_command_, "next") == 0 || std::strcmp(usb_command_, "previous") == 0 || std::strcmp(usb_command_, "more") == 0) {
                        const auto data = home::Snapshot();
                        Acknowledge();
                        Navigate(std::strcmp(usb_command_, "next") == 0 ? ZectrixButton::kDown : std::strcmp(usb_command_, "previous") == 0 ? ZectrixButton::kUp : ZectrixButton::kOk, data);
                        navigation_requested_ = true;
                    }
                    if (std::strcmp(usb_command_, "refresh") == 0) { Acknowledge(); home::RefreshFeeds(); }
                }
                command_length_ = 0; command_overflow_ = false; usb_command_[0] = 0;
            } else if (command_length_ + 1 < sizeof(usb_command_)) {
                usb_command_[command_length_++] = c; usb_command_[command_length_] = 0;
            } else command_overflow_ = true;
        }
    }
    void Display(bool wait = false) {
        const auto ticket = display_.Submit(canvas_.data());
        if (wait) display_.Wait(ticket);
    }
    void Message(const char* title, const std::vector<std::string>& lines) {
        home::RenderMessage(canvas_, title, lines); Display(true);
    }
    void Setup() {
        home::SetupInfo info;
        ESP_ERROR_CHECK(home::StartSetup(info));
        Message("Let's get connected", {"1. Join Wi-Fi: " + info.ssid,
            "   Password: " + info.password,
            "2. Open http://192.168.4.1",
            "3. Save Wi-Fi and calendar settings.",
            "Keep this screen's password private.", "Hold DOWN to power off."});
        while (true) {
            ZectrixButtonEvent event;
            BatteryTelemetry();
            if (board_.WaitButton(&event, pdMS_TO_TICKS(200)) && event.button == ZectrixButton::kDown && event.action == ZectrixButtonAction::kLongPress) Shutdown();
        }
    }
    void Shutdown() {
        RecordBattery();
        Message("See you soon", {"Home Note4 is powered down.", "Unplug USB to switch off fully."});
        esp_wifi_stop(); board_.SetAudioPower(false); board_.SetPowerLed(false);
        board_.CutBatteryPower(); esp_deep_sleep_start();
    }
    home::DisplayWorker display_;
    int section_ = 0, detail_page_ = 0;
    int64_t led_until_ = 0;
    bool dashboard_ready_ = false, navigation_requested_ = false;
    home::BatteryHistory battery_history_;
    uint32_t boot_id_ = 0;
    int64_t next_battery_sample_ = 10000000; // Allow charger signals to settle after boot.
    char usb_command_[32] = {};
    size_t command_length_ = 0;
    bool command_overflow_ = false, usb_commands_ = false;
    ZectrixBoard board_;
    ZectrixCanvas canvas_;
    zectrix_epd_handle_t epd_ = nullptr;
};
}
extern "C" void app_main() { static HomeNote app; app.Run(); }
