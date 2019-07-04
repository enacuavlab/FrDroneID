 /*
  Drone Sniffer
  
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "string.h"
#include "drone_sniffer.h"

#define MIN(a,b) (a<b?a:b)



static const char *TAG = "SNIFFER";

static const uint8_t CID_french_defense[] = {0x6A, 0x5C, 0x35};

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  ESP_LOGI(TAG, "Event : %d",event->event_id);
  return ESP_OK;
}

uint8_t get_element_info(uint8_t* buf, int max_len, uint8_t* type) {
  if(max_len<2) {
    return 0;
  }
  *type = buf[0];
  uint8_t len = buf[1];
  
  if(max_len < len+2) {
    return 0; //return max_len ???
  }
  
  return len;
}

char* read_SSID(uint8_t* buf, uint8_t ssid_len) {
  
  char *ssid = malloc ((ssid_len+1)*sizeof(char));
	for(int i=0; i<ssid_len; i++) {
	  ssid[i]=buf[i];
	}
	ssid[ssid_len]=0;
  return ssid;
}

struct uas_raw_payload read_uav_info(uint8_t* buf, uint8_t vs_type, uint8_t len) {
  
  struct uas_raw_payload payload = {0};   //init empty payload
  payload.id_fr[30] = '\0';
  int offset = 0;
  while(offset < len) {
    enum uas_type field_type = (enum uas_type) buf[offset];
    int field_len = buf[offset+1];
    
    if(offset + field_len + 2 > len) {  //is the announced field length compatible with the buffer length ?
      break;
    }
    
    void *dst = NULL;
    int size = 0;
    switch(field_type) {    //get address of the field
      case UAS_ID_FR:
        dst = &payload.id_fr;
        size = sizeof(payload.id_fr) - 1; //keep room for the \0 (end of string)
        break;
      case UAS_LAT:
        dst = &payload.lat;
        size = sizeof(payload.lat);
        break;
      case UAS_LON:
        dst = &payload.lon;
        size = sizeof(payload.lon);
        break;
      case UAS_HMSL:
        dst = &payload.hmsl;
        size = sizeof(payload.hmsl);
        break;
      case UAS_HAGL:
        dst = &payload.hagl;
        size = sizeof(payload.hagl);
        break;
      case UAS_LAT_TO:
        dst = &payload.lat_to;
        size = sizeof(payload.lat_to);
        break;
      case UAS_LON_TO:
        dst = &payload.lon_to;
        size = sizeof(payload.lon_to);
        break;
      case UAS_H_SPEED:
        dst = &payload.h_speed;
        size = sizeof(payload.h_speed);
        break;
      case UAS_ROUTE:
        dst = &payload.route;
        size = sizeof(payload.route);
        break;
      case UAS_PROTOCOL_VERSION:    // handle it ? For now only 1 version exists
        size = 0;
        break;
      case UAS_ID_ANSI_UAS:         // not supported, as 
        size = 0;
        break;
    }
    if(dst != NULL && field_len <= size) {      // check if received length match struct field length
      memcpy(dst, buf+offset+2, field_len);    // copy data in structure
      payload.types |= (1<<field_type);         // set field flag
    } else {
      printf("Field length is longer than sizeof(field) !!!!\n");
    }
    
    offset += field_len+2;
  }
  
  
  return payload;
}

void display_info(struct uas_raw_payload* info) {
  if(info->types & (1<<UAS_ID_FR)) {
    printf("FR_ID: %s\n", info->id_fr);
  }
  if(info->types & (1<<UAS_LAT)) {
    printf("LAT: %f\n", info->lat/1e5);
  }
  if(info->types & (1<<UAS_LON)) {
    printf("LON: %f\n", info->lon/1e5);
  }
  if(info->types & (1<<UAS_HMSL)) {
    printf("HMSL: %d\n", info->hmsl);
  }
  if(info->types & (1<<UAS_HAGL)) {
    printf("HAGL: %d\n", info->hagl);
  }
  if(info->types & (1<<UAS_LAT_TO)) {
    printf("LAT TO: %f\n", info->lat_to/1e5);
  }
  if(info->types & (1<<UAS_LON_TO)) {
    printf("LON TO: %f\n", info->lon_to/1e5);
  }
  if(info->types & (1<<UAS_H_SPEED)) {
    printf("H SPEED: %d\n", info->h_speed);
  }
  if(info->types & (1<<UAS_ROUTE)) {
    printf("ROUTE: %d\n", info->route);
  }
  
  printf("\n");
}

void wifi_promiscuous_cb (void *buf, wifi_promiscuous_pkt_type_t type)
{
  wifi_promiscuous_pkt_t *pkt=(wifi_promiscuous_pkt_t *)buf;
  if(type == WIFI_PKT_MGMT) {
    //uint8_t protocol_version = (pkt->payload[0]&0x03);
    uint8_t frame_type = (pkt->payload[0]&0x0C)<<2;
    frame_type += (pkt->payload[0]&0xF0)>>4;
    
    if (frame_type==0x08) {
      char* ssid = NULL;
      int offset = 36;
      while(true) {
        uint8_t e_type;
        uint8_t len = get_element_info(pkt->payload+offset, pkt->rx_ctrl.sig_len - offset, &e_type);
        if(len == 0) {
          break;
        }
        
        if(e_type == 0) {   //SSID
          ssid = read_SSID(pkt->payload+offset+2, len);
        }
        else if(e_type == 0XDD) { //Vendor Specific
          uint8_t CID[3];
          memcpy(CID, pkt->payload+offset+2, 3);
          uint8_t vs_type = pkt->payload[offset+5];
          bool same = true;
          for(int i=0; i<3; i++) {
            if(CID[i] != CID_french_defense[i]) { same = false; }
          }
          if(same) {
            struct uas_raw_payload raw_info = read_uav_info(pkt->payload+offset+6, vs_type, len-4);
            printf("SSID = %s\n", ssid);
            printf("raw info received = %04X\n", raw_info.types);
            display_info(&raw_info);
          }
        }
        
        offset += len+2;
      }
          
    }
  }
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    tcpip_adapter_init();
    
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    wifi_country_t country_conf;
    country_conf.schan=1;
    country_conf.nchan=13;
    country_conf.policy=WIFI_COUNTRY_POLICY_AUTO;
    
    ESP_ERROR_CHECK(esp_wifi_set_country(&country_conf));

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    ESP_ERROR_CHECK(esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE));
    
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb));

    ESP_ERROR_CHECK(esp_wifi_start());

    while(true)
    {
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}
