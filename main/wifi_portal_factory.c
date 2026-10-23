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
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>PRISMA-PI Simple Synth</title><style>*{box-sizing:border-box}body{margin:0;background:#05080d;color:#eef7ff;font:14px system-ui,Arial;padding:22px}main{max-width:760px;margin:auto}h1{letter-spacing:3px;margin:0;font-size:24px}.c{color:#28dddd}.sub{color:#8ea6b8;letter-spacing:2px;margin:5px 0 20px}.card{border:1px solid #263545;border-radius:14px;padding:18px;background:#091019;margin:12px 0}.row{display:flex;justify-content:space-between;gap:12px;margin:10px 0;font-family:monospace}.v{color:#28dddd;font-weight:700}.live{display:inline-block;padding:6px 10px;border:1px solid #28dddd;border-radius:20px;color:#28dddd}a{color:#28dddd;text-decoration:none;margin-left:16px}</style></head>"
        "<body><main><div style='display:flex;justify-content:space-between;align-items:center'><div><h1>PRISMA-PI <span class='c'>SIMPLE SYNTH</span></h1><div class='sub'>PUBLIC READ-ONLY MONITOR</div></div><span id='live' class='live'>CONNECTING</span></div>"
        "<section class='card'><div class='row'><span>NOTE</span><span id='note' class='v'>--</span></div><div class='row'><span>FREQUENCY</span><span id='hz' class='v'>--</span></div><div class='row'><span>ROOT / SCALE</span><span id='scale' class='v'>--</span></div><div class='row'><span>WAVEFORM</span><span id='wave' class='v'>--</span></div><div class='row'><span>OUTPUT</span><span id='out' class='v'>--</span></div><div class='row'><span>ORIENTATION</span><span id='angles' class='v'>--</span></div></section>"
        "<div style='text-align:right'><a href='/simple-synth'>SIMPLE SYNTH</a><a href='/admin'>ADMIN</a></div>"
        "<script>async function tick(){try{let r=await fetch('/api/sensors/imu',{cache:'no-store'}),d=await r.json();if(!d.sample)throw 0;let b=d.beep;live.textContent='LIVE';note.textContent=b.note;hz.textContent=Number(b.hz).toFixed(2)+' HZ';scale.textContent=b.root_name+' / '+b.scale_name;wave.textContent=b.waveform_name;out.textContent=b.master?(b.active?'PLAYING':'MUTED'):'STOPPED';angles.textContent='X '+d.angle.x.toFixed(1)+'  Y '+d.angle.y.toFixed(1)+'  Z '+d.angle.z.toFixed(1)}catch(e){live.textContent='NO SAMPLE'}}setInterval(tick,150);tick()</script></main></body></html>";
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, page, HTTPD_RESP_USE_STRLEN);
}

void prisma_wifi_start(void)
{
    esp_err_t net = prisma_network_init();
    if (net != ESP_OK) {
        ESP_LOGE(TAG, "network init failed: %s", esp_err_to_name(net));
        return;
    }

    esp_err_t sec = prisma_security_init();
    if (sec != ESP_OK) {
        ESP_LOGE(TAG, "security init failed: %s", esp_err_to_name(sec));
        return;
    }

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
        /* Register exact media/download transport before the legacy Files+ wildcard. */
        prisma_playback_http_register(server);
        prisma_files_plus_register(server);
        prisma_audio_api_register(server);
    }

    prisma_network_status_t status;
    prisma_network_get_status(&status);
    ESP_LOGI(TAG, "%s ready: / read-only, /simple-synth instrument, /admin system", status.ap_ssid);
}
