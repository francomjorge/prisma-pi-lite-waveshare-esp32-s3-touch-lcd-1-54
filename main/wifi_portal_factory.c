#include "wifi_portal.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "admin_api.h"
#include "audio_api.h"
#include "files_plus_api.h"
#include "playback_http.h"
#include "sensors_web.h"
#include "network_service.h"
#include "security_service.h"

static const char *TAG = "ADMIN_WIFI";

static esp_err_t root_get(httpd_req_t *r)
{
    static const char page[] =
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>"
        "<title>PRISMA-PI Simple Synth</title><style>"
        "*{box-sizing:border-box}html,body{min-height:100%}body{margin:0;background:#03070d;color:#eef7ff;font:14px system-ui,Arial;display:flex;align-items:center;justify-content:center;padding:22px}"
        ".shell{width:min(900px,100%);border:1px solid #284a73;border-radius:22px;background:radial-gradient(circle at 50% 42%,#0b1b2c 0,#07101b 42%,#03070d 78%);padding:38px 30px 24px;box-shadow:0 30px 90px #000a;overflow:hidden}"
        ".hero{text-align:center}.logo{font:900 clamp(42px,8vw,72px) monospace;letter-spacing:3px;line-height:1;margin:0;color:#fff;text-shadow:0 0 12px #dff5ff66}.logo .fx{display:inline-block;color:#53eaff;text-shadow:-5px 0 10px #ff36d1,5px 0 10px #13dfff,0 0 18px #7a5cff;animation:prism 2.6s ease-in-out infinite alternate}.product{font:800 clamp(22px,4.5vw,36px) monospace;letter-spacing:8px;color:#fff;margin:10px 0 24px}"
        "@keyframes prism{0%{text-shadow:-4px 0 8px #ff35cf,4px 0 10px #13dfff,0 0 12px #5b65ff}50%{text-shadow:-2px 0 13px #6d54ff,3px 0 14px #00f0ff,0 0 18px #ff35cf}100%{text-shadow:-5px 0 11px #00d9ff,5px 0 12px #ff35cf,0 0 17px #7357ff}}"
        ".appliance{width:min(430px,88%);margin:0 auto 22px;position:relative;filter:drop-shadow(0 20px 24px #000b)}.case{aspect-ratio:1.38/1;border-radius:36px;background:linear-gradient(145deg,#f4d8d4,#cba8a6 48%,#76686d 100%);padding:20px;box-shadow:inset 0 0 0 2px #ffe6e0aa,12px 4px 30px #00d9ff20,-10px 4px 28px #ff35cf18}.bezel{width:100%;height:100%;border-radius:25px;background:#020307;padding:18px}.lcd{height:100%;border-radius:4px;background:radial-gradient(circle at 55% 44%,#14376a 0,#06152e 45%,#020711 100%);position:relative;overflow:hidden;border:1px solid #0e2945;color:#34e8ff;font:700 12px monospace;text-align:left}.lcdtop{position:absolute;left:12px;right:12px;top:10px;display:flex;justify-content:space-between}.fractal{position:absolute;width:44%;aspect-ratio:1;left:39%;top:24%;border-radius:42% 58% 55% 45%;background:radial-gradient(circle at 55% 50%,#08152d 0 12%,#1ce7ff 15%,#4755ff 28%,#fa31d0 42%,transparent 58%);filter:blur(.2px) drop-shadow(0 0 10px #19dfff);transform:rotate(22deg)}.fractal:before,.fractal:after{content:'';position:absolute;inset:17%;border:2px solid #39efff;border-radius:38% 62% 45% 55%;transform:rotate(58deg)}.fractal:after{inset:29%;border-color:#ff42d5;transform:rotate(-35deg)}.note{position:absolute;left:16px;bottom:38px;font:900 42px monospace;color:#e8fbff;text-shadow:0 0 12px #29dfff}.lcdfoot{position:absolute;left:12px;right:12px;bottom:10px;display:flex;justify-content:space-between}"
        ".copy{text-align:center;max-width:720px;margin:0 auto 22px;color:#c2d1dc;font:15px monospace;line-height:1.55}.copy strong{color:#27e6ee}.actions{text-align:center}.btn{display:flex;align-items:center;justify-content:center;text-decoration:none;margin:0 auto;border:1px solid #2debf0;color:#2debf0;background:#06131b;border-radius:14px;font:800 16px monospace;letter-spacing:2px;transition:.15s}.btn:hover,.btn:focus{transform:translateY(-1px);box-shadow:0 0 18px #22e7ed55}.synth{width:min(420px,90%);padding:17px 22px;box-shadow:0 0 15px #22e7ed33}.admin{width:min(230px,64%);padding:11px 18px;margin-top:12px;color:#a7c5d4;border-color:#496b7c;font-size:13px}.admin:hover,.admin:focus{border-color:#2debf0;color:#2debf0}.foot{border-top:1px solid #285170;margin:28px auto 0;padding-top:19px;text-align:center;color:#66859b;font:10px monospace;letter-spacing:3px}"
        "@media(max-width:620px){body{padding:8px}.shell{padding:28px 14px 18px}.product{letter-spacing:5px}.case{border-radius:25px;padding:13px}.bezel{border-radius:18px;padding:12px}.lcd{font-size:9px}.note{font-size:30px;bottom:30px}.copy{font-size:12px}.foot{letter-spacing:1.6px}}"
        "</style></head><body><main class='shell'><section class='hero'>"
        "<h1 class='logo'>PRISMA<span class='fx'>-PI</span></h1><div class='product'>SIMPLE SYNTH</div>"
        "<div class='appliance' aria-label='PRISMA-PI Simple Synth appliance'><div class='case'><div class='bezel'><div class='lcd'><div class='lcdtop'><span>SIMPLE SYNTH</span><span>120.0 BPM</span></div><div class='fractal'></div><div class='note'>A</div><div class='lcdfoot'><span>A / MIN</span><span>SQUARE</span><span>G01</span></div></div></div></div></div>"
        "<div class='copy'>Synthesizer, generative visuals and sensor-driven instrument.<br>Play, explore and create with motion, <strong>sound</strong> and <strong>light</strong>.</div>"
        "<div class='actions'><a class='btn synth' href='/simple-synth'>SIMPLE SYNTH&nbsp;&nbsp;›</a><a class='btn admin' href='/admin'>ADMIN&nbsp;&nbsp;›</a></div>"
        "<div class='foot'>VUDUM DEVELOPMENTS · PRISMA-PI SIMPLE SYNTH</div></section></main></body></html>";
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, page, HTTPD_RESP_USE_STRLEN);
}

void prisma_wifi_start(void)
{
    esp_err_t net = prisma_network_init();
    if (net != ESP_OK) { ESP_LOGE(TAG, "network init failed: %s", esp_err_to_name(net)); return; }
    esp_err_t sec = prisma_security_init();
    if (sec != ESP_OK) { ESP_LOGE(TAG, "security init failed: %s", esp_err_to_name(sec)); return; }
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.lru_purge_enable = true;
    hc.max_uri_handlers = 48;
    hc.stack_size = 12288;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &hc) == ESP_OK) {
        httpd_uri_t root = {.uri="/",.method=HTTP_GET,.handler=root_get};
        httpd_register_uri_handler(server, &root);
        prisma_admin_register(server);
        prisma_sensors_web_register(server);
        prisma_playback_http_register(server);
        prisma_files_plus_register(server);
        prisma_audio_api_register(server);
    }
    prisma_network_status_t status; prisma_network_get_status(&status);
    ESP_LOGI(TAG, "%s ready: / home, /simple-synth instrument, /admin system", status.ap_ssid);
}
