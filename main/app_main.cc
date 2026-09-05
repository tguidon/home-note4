#include "feeds.h"
#include "network.h"
#include "render.h"
#include <ctime>
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
        Message("Starting up", {"Weather for Wilton, CT 06897", "Formula 1 and NFL", "Connecting to your home..."});
        auto err = home::InitNetwork();
        if (err != ESP_OK) {
            Message("Storage needs attention", {"Settings could not be read.", "Nothing has been erased.", "Check the USB serial log."});
            ESP_LOGE("home_note", "network init: %s", esp_err_to_name(err)); return;
        }
        if (!home::HasWifiCredentials() || !home::ConnectWifi(30000)) { Setup(); return; }
        home::StartClock(); home::StartFeeds();
        int page = 0; unsigned revision = ~0u; bool online = false;
        time_t shown_day = 0, shown_interval = 0; int64_t last_draw = -10000000; bool redraw = true;
        while (true) {
            auto data = home::Snapshot(); const time_t now = time(nullptr);
            tm local = {}; localtime_r(&now, &local);
            const time_t day = (local.tm_year + 1900) * 400 + local.tm_yday;
            page %= home::PageCount(data);
            if (data.revision != revision || online != home::WifiConnected() || day != shown_day || now / 300 != shown_interval) redraw = true;
            if (redraw && esp_timer_get_time() - last_draw >= 3000000) {
                online = home::WifiConnected();
                home::RenderDashboard(canvas_, data, page, now, online);
                Display(); last_draw = esp_timer_get_time(); redraw = false;
                revision = data.revision; shown_day = day; shown_interval = now / 300;
            }
            ZectrixButtonEvent event;
            if (!board_.WaitButton(&event, pdMS_TO_TICKS(200))) continue;
            if (event.action == ZectrixButtonAction::kLongPress) {
                if (event.button == ZectrixButton::kDown) Shutdown();
                if (event.button == ZectrixButton::kOk) home::SetupOnNextBoot();
                continue;
            }
            if (event.button == ZectrixButton::kUp) page = (page + home::PageCount(data) - 1) % home::PageCount(data);
            else if (event.button == ZectrixButton::kDown) page = (page + 1) % home::PageCount(data);
            else home::RefreshFeeds();
            redraw = true;
        }
    }
private:
    void Display() {
        // V1 uses full 1bpp refreshes only. Always remove panel power when idle.
        esp_err_t err = zectrix_epd_power_on(epd_);
        if (err == ESP_OK) err = zectrix_epd_refresh_full_1bpp(epd_, canvas_.data(), canvas_.size());
        zectrix_epd_power_off(epd_);
        if (err != ESP_OK) ESP_LOGE("home_note", "display refresh: %s", esp_err_to_name(err));
    }
    void Message(const char* title, const std::vector<std::string>& lines) {
        home::RenderMessage(canvas_, title, lines); Display();
    }
    void Setup() {
        home::SetupInfo info;
        ESP_ERROR_CHECK(home::StartSetup(info));
        Message("Let's get connected", {"1. Join Wi-Fi: " + info.ssid,
            "   Password: " + info.password,
            "2. Open http://192.168.4.1",
            "3. Enter your home 2.4 GHz Wi-Fi.",
            "Keep this screen's password private.", "Hold DOWN to power off."});
        while (true) {
            ZectrixButtonEvent event;
            if (board_.WaitButton(&event, portMAX_DELAY) && event.button == ZectrixButton::kDown && event.action == ZectrixButtonAction::kLongPress) Shutdown();
        }
    }
    void Shutdown() {
        Message("See you soon", {"Home Note4 is powered down.", "Unplug USB to switch off fully."});
        esp_wifi_stop(); board_.SetAudioPower(false); board_.SetPowerLed(false);
        board_.CutBatteryPower(); esp_deep_sleep_start();
    }
    ZectrixBoard board_;
    ZectrixCanvas canvas_;
    zectrix_epd_handle_t epd_ = nullptr;
};
}
extern "C" void app_main() { static HomeNote app; app.Run(); }
