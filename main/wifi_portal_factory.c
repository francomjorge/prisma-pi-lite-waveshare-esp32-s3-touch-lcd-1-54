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
    prisma_network_status_t status;
    prisma_network_get_status(&status);
    char page[7600];
    snprintf(page, sizeof(page),
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1,viewport-fit=cover'>"
        "<title>PRISMA-PI Simple Synth</title><style>"
        "*{box-sizing:border-box}html,body{min-height:100%%}body{margin:0;background:#05080d;color:#eef7ff;font:14px system-ui,Arial;display:flex;align-items:center;justify-content:center;padding:22px}"
        ".shell{width:min(820px,100%%)}.device{border:1px solid #263545;border-radius:20px;background:linear-gradient(145deg,#0b131c,#070c12);padding:28px;box-shadow:0 24px 80px #0008;position:relative;overflow:hidden}"
        ".device:before{content:'';position:absolute;inset:0;background:linear-gradient(90deg,transparent 49.7%%,#28dddd10 50%%,transparent 50.3%%);pointer-events:none}"
        ".brand{font-size:13px;letter-spacing:4px;color:#7f96a7;margin-bottom:18px}.logo{font:800 clamp(34px,7vw,62px) monospace;letter-spacing:4px;line-height:.95;margin:0}.logo span{color:#28dddd}.product{font:800 clamp(18px,4vw,30px) monospace;letter-spacing:8px;color:#28dddd;margin:9px 0 4px}.maker{font:11px monospace;letter-spacing:3px;color:#708696;margin-bottom:28px}"
        ".screen{border:1px solid #314858;border-radius:13px;background:#02070a;padding:20px;box-shadow:inset 0 0 35px #28dddd08}.screen h2{font:700 12px monospace;letter-spacing:3px;color:#28dddd;margin:0 0 12px}.screen p{margin:0;color:#b8c9d5;line-height:1.65}.routes{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:18px}.route{display:block;border:1px solid #263f4d;border-radius:12px;padding:17px;text-decoration:none;color:#eef7ff;background:#091019;transition:.15s}.route:hover,.route:focus{border-color:#28dddd;transform:translateY(-1px)}.route b{display:block;color:#28dddd;font:800 13px monospace;letter-spacing:2px;margin-bottom:7px}.route small{color:#8da2b1;line-height:1.4}"
        ".net{margin-top:18px;border-top:1px solid #182631;padding-top:15px;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px 18px}.n{display:flex;justify-content:space-between;gap:12px;font:11px monospace;color:#718898}.n strong{color:#b9ccd8;font-weight:600;text-align:right}.foot{margin-top:17px;text-align:center;color:#4f6574;font:10px monospace;letter-spacing:2px}.dot{display:inline-block;width:7px;height:7px;border-radius:50%%;background:#28dddd;box-shadow:0 0 10px #28dddd;margin-right:7px}"
        "@media(max-width:620px){body{padding:10px}.device{padding:20px}.routes{grid-template-columns:1fr}.net{grid-template-columns:1fr}.product{letter-spacing:5px}}"
        "</style></head><body><main class='shell'><section class='device'>"
        "<div class='brand'>PRISMA SERIES</div><h1 class='logo'>PRISMA<span>-PI</span></h1><div class='product'>SIMPLE SYNTH</div><div class='maker'>A VOODOO DEVELOPMENTS INSTRUMENT</div>"
        "<div class='screen'><h2><span class='dot'></span>SYSTEM READY</h2><p>Sensor-driven synthesizer and generative visual instrument. Use <b>/simple-synth</b> to play and control the instrument. Use <b>/admin</b> for appliance configuration and system administration.</p>"
        "<div class='routes'><a class='route' href='/simple-synth'><b>SIMPLE SYNTH</b><small>Play the instrument, control G01 visuals, audio, scales, waveforms and live sensor response.</small></a><a class='route' href='/admin'><b>ADMIN</b><small>Manage network, appliance settings, files and system information.</small></a></div>"
        "<div class='net'><div class='n'><span>NETWORK</span><strong>%s</strong></div><div class='n'><span>ADDRESS</span><strong>192.168.4.1</strong></div></div></div>"
        "<div class='foot'>VOODOO DEVELOPMENTS · PRISMA-PI SIMPLE SYNTH</div></section></main></body></html>", status.ap_ssid);
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
