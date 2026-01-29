/**
 * @file    main.c
 *
 * @author  Sumit Paropkari
 * @owner   Sumit Paropkari
 * @email   sumit.paropkari@wheelseye.com
 *
 * @version 1.0.0
 * @date    29-Jan-2026
 *
 * @copyright
 * Copyright (c) 2026 Sumit Paropkari.
 * All rights reserved.
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/device.h>
#include "agota.h"

#define FW_START_ADDRESS    0x000B6000 //fw start pointer
#define FW_SIZE             548656
#define INITIAL_READ_SIZE   10
#define MTU_SIZE            247

static const char target_mac[] = "68:67:25:E8:71:FE";
static bt_addr_le_t target_addr={.type = BT_ADDR_LE_PUBLIC, .a.val = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static bool ag_data_ready = false;
static bool ota_ready = false;
static bool ota_done = false;

static struct bt_conn *default_conn = NULL;

static struct bt_gatt_discover_params discover_params;

static uint16_t service_end_handle;

/* OTA notify characteristic */
static uint16_t ota_cmd_value_handle;
static uint16_t ota_data_value_handle;
static uint16_t ota_notify_cccd_handle;
static struct bt_gatt_subscribe_params ota_sub_params;

/* AG notify characteristic */
static uint16_t ag_value_handle;
static uint16_t ag_notify_cccd_handle;
static struct bt_gatt_subscribe_params ag_sub_params;
static uint8_t tx_data[] = {0x78,0x78,0x01,0x0C,0x08,0x63,0x49,0x20,0x50,0x16,0x74,0x56,0x02,0x25,0x82,0xEB,0x0D,0x0A};

static struct bt_uuid_128 ota_service = BT_UUID_INIT_128( 0xd8, 0xe6, 0xfd, 0x1d, 0x4a, 0x14, 0xc6, 0xb1, 0x53, 0x4c, 0x4c, 0x59, 0x6d, 0xd9, 0xf1, 0xd6 );
static struct bt_uuid_128 ota_cmd_char_uuid = BT_UUID_INIT_128( 0x30, 0xd8, 0xe3, 0x3a, 0x0e, 0x27, 0x22, 0xb7, 0xa4, 0x46, 0xc0, 0x21, 0xaa, 0x71, 0xd6, 0x7a );
static struct bt_uuid_128 ota_data_char_uuid = BT_UUID_INIT_128( 0xb0, 0xa5, 0xf8, 0x45, 0x8d, 0xca, 0x89, 0x9b, 0xd8, 0x4c, 0x40, 0x1f, 0x88, 0x88, 0x40, 0x23 );
static struct bt_uuid_128 ag_service = BT_UUID_INIT_128( 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00 );
static struct bt_uuid_128 ag_data_char_uuid = BT_UUID_INIT_128( 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00 );

// static struct bt_uuid_128 ota_service;
// static struct bt_uuid_128 ota_cmd_char_uuid;
// static struct bt_uuid_128 ota_data_char_uuid;
// static struct bt_uuid_128 ag_service;
// static struct bt_uuid_128 ag_data_char_uuid;

static uint16_t current_service;
static uint16_t current_characteristic;

static void start_scan(void);
static void device_found(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad);
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
static void exchange_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params);
// static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);

void start_gatt_discovery(struct bt_conn *conn);
static uint8_t discover_service_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);
static uint8_t discover_char_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);
static uint8_t discover_cccd_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);

void enable_ota_notifications(void);
void enable_ag_notifications(void);

static void init_target_addr(void);

static struct bt_le_scan_cb scan_cb = {
    .recv = device_found,
};

BT_CONN_CB_DEFINE(conn_cb) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void init_target_addr(void)
{
    bt_addr_from_str(target_mac, &target_addr.a);
    printf("Target addr: %s\n", target_mac);
}

static void start_scan(void)
{
    int err;
    err = bt_enable(NULL);
    if (err) {
        printf("Bluetooth init failed (err %d)\n", err);
        return;
    }
    struct bt_le_scan_param scan_param = {
        .type = BT_HCI_LE_SCAN_ACTIVE, //BT_LE_SCAN_TYPE_PASSIVE
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = 0x0060,
        .window = 0x0030,
    };
    bt_le_scan_stop();
    k_msleep(100);
    bt_le_scan_cb_register(&scan_cb);
    err = bt_le_scan_start(&scan_param, NULL);
    if (err)
    {
        printf("Scanning failed to start (err %d)\n", err);
        return;
    }
    printf("Scanning started successfully\n");
}

static void device_found(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));
    printf("DEVICE------------ %s (RSSI %d)\n", addr_str, info->rssi);
	if (!bt_addr_le_cmp(info->addr, &target_addr))
    {
        printf("Target Found ------------ %s (RSSI %d)\n", addr_str, info->rssi);
        bt_le_scan_stop();
        bt_conn_le_create(info->addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    if (err) {
        printf("Connection failed (%d)\n", err);
        start_scan();
        return;
    }
    printf("Connected to %s \n", addr);
    default_conn = bt_conn_ref(conn);
    printf("start mtu excange\n");
    static struct bt_gatt_exchange_params exchange_params;
	exchange_params.func = exchange_func;
	err = bt_gatt_exchange_mtu(conn, &exchange_params);
	if (err) {
        printf("MTU exchange failed (err %d)", err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printf("Disconnected (%d)\n", reason);
    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
    start_scan();
}

static void exchange_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params)
{
    if (err) {
        printf("MTU exchange failed (%d)\n", err);
        return;
    }
    printf("MTU exchanged done\n");

    start_gatt_discovery(conn);
}

static uint8_t discover_service_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{

    if (!attr) {
        printf("Service discovery complete\n");
        return BT_GATT_ITER_STOP;
    }

    struct bt_gatt_service_val *service = attr->user_data;

    char uuid_str[BT_UUID_STR_LEN];
    bt_uuid_to_str(service->uuid, uuid_str, sizeof(uuid_str));
    
    current_service  = attr->handle;
    printf("Service found: handle=0x%04x end=0x%04x UUID=%s\n",
           current_service, service->end_handle, uuid_str);

    /* CHECK if this is your custom service */
    if (!bt_uuid_cmp(service->uuid, &ota_service.uuid)) {
        printf(">>> OTA service matched!\n");
        service_end_handle   = service->end_handle;
        /* Now discover characteristics INSIDE this service */
        discover_params.uuid = &ota_cmd_char_uuid.uuid;
        discover_params.start_handle = current_service + 1;
        discover_params.end_handle   = service_end_handle;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        discover_params.func = discover_char_cb;
        bt_gatt_discover(conn, &discover_params);

        return BT_GATT_ITER_CONTINUE; // STOP only after target found
    }

    if (!bt_uuid_cmp(service->uuid, &ag_service.uuid)) {
        printf(">>> AG service matched!\n");
        service_end_handle   = service->end_handle;
        /* Now discover characteristics INSIDE this service */
        discover_params.uuid = &ag_data_char_uuid.uuid;
        discover_params.start_handle = current_service + 1;
        discover_params.end_handle   = service_end_handle;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        discover_params.func = discover_char_cb;

        bt_gatt_discover(conn, &discover_params);

        return BT_GATT_ITER_STOP; // STOP only after target found
    }


    return BT_GATT_ITER_CONTINUE; // keep scanning other services

}

static uint8_t discover_char_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printf("Characteristic discovery complete\n");
        
        return BT_GATT_ITER_STOP;
    }
    struct bt_gatt_chrc *chrc = attr->user_data;
    current_characteristic  = attr->handle;
    char uuid_str[BT_UUID_STR_LEN];
    bt_uuid_to_str(chrc->uuid, uuid_str, sizeof(uuid_str));
    printf("Char found: handle=0x%04x UUID=%s props=0x%02x\n", chrc->value_handle, uuid_str, chrc->properties);
    if (!bt_uuid_cmp(chrc->uuid, &ota_cmd_char_uuid.uuid)) {
        printf(">>> OTA cmd char matched!\n");
        ota_cmd_value_handle = chrc->value_handle;
    }
    if (!bt_uuid_cmp(chrc->uuid, &ota_data_char_uuid.uuid)) {
        printf(">>> OTA data char matched!\n");
        ota_data_value_handle = chrc->value_handle;
    }
    if (!bt_uuid_cmp(chrc->uuid, &ag_data_char_uuid.uuid)) {
        printf(">>> AG char matched!\n");
        ag_value_handle = chrc->value_handle;   
    }
    if (chrc->properties & BT_GATT_CHRC_NOTIFY){
        /* Discover CCCD */
        discover_params.uuid = BT_UUID_GATT_CCC;
        discover_params.start_handle = chrc->value_handle + 1;
        discover_params.end_handle   = service_end_handle;
        discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
        discover_params.func = discover_cccd_cb;

        bt_gatt_discover(conn, &discover_params);

        return BT_GATT_ITER_STOP;
    }
    else {
        discover_params.uuid = chrc->uuid;
        discover_params.start_handle = chrc->value_handle + 1;
        discover_params.end_handle   = service_end_handle;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        discover_params.func = discover_char_cb;
    }
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_cccd_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printf("CCCD discovery complete\n");
        return BT_GATT_ITER_STOP;
    }
    if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC)) {
        if (ota_cmd_value_handle && attr->handle > ota_cmd_value_handle && ota_notify_cccd_handle == 0) {
            ota_notify_cccd_handle = attr->handle;
            printf("OTA CCCD found: handle=0x%04x\n", ota_notify_cccd_handle);
            enable_ota_notifications();
            return BT_GATT_ITER_STOP;
        }
        if (ag_value_handle && attr->handle > ag_value_handle && ag_notify_cccd_handle == 0) {
            ag_notify_cccd_handle = attr->handle;
            printf("AG CCCD found: handle=0x%04x\n", ag_notify_cccd_handle);
            enable_ag_notifications();
            return BT_GATT_ITER_STOP;
        }
    }
    // cccd_handle = attr->handle;
    // printf("CCCD found: handle=0x%04x\n", cccd_handle);
    // enable_notifications();
    return BT_GATT_ITER_STOP;
}

void start_gatt_discovery(struct bt_conn *conn)
{
    default_conn = conn;
    discover_params.uuid = NULL;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    discover_params.func = discover_service_cb;
    bt_gatt_discover(conn, &discover_params);
}

static uint8_t ota_notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length)
{
    if (!data) {
        printf("ota Notifications stopped\n");
        return BT_GATT_ITER_STOP;
    }
    printf("Notify (%u bytes): ", length);
    for (int i = 0; i < length; i++) {
        printf("%02x ", ((uint8_t *)data)[i]);
    }
    printf("\n");
    memcpy (response,data,length);
    printf("response -> ");
    for(int i=0; i<length;i++)
    {
        printf ("%02x ", response[i]);
    }
    printf("\r\n");
    return BT_GATT_ITER_CONTINUE;
}


static uint8_t ag_notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length)
{
    if (!data) {
        printf("ag Notifications stopped\n");
        return BT_GATT_ITER_STOP;
    }
    printf("Notify (%u bytes): ", length);
    for (int i = 0; i < length; i++) {
        printf("%02x ", ((uint8_t *)data)[i]);
    }
    printf("\n");
    return BT_GATT_ITER_CONTINUE;
}

void enable_ota_notifications(void)
{
    ota_sub_params.notify = ota_notify_cb;
    ota_sub_params.value_handle = ota_cmd_value_handle;
    ota_sub_params.ccc_handle = ota_notify_cccd_handle;
    ota_sub_params.value = BT_GATT_CCC_NOTIFY;
    bt_gatt_subscribe(default_conn, &ota_sub_params);
    printf("OTA notifications enabled\n");
    ota_ready = true;
    // return BT_GATT_ITER_STOP;

    // discover_params.uuid = NULL;
    // discover_params.start_handle = current_service + 1;
    // discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    // discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    // discover_params.func = discover_service_cb;
    // bt_gatt_discover(default_conn, &discover_params);

    discover_params.uuid = NULL;
    discover_params.start_handle = current_characteristic + 1;
    discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = discover_char_cb;
    bt_gatt_discover(default_conn, &discover_params);
}

void enable_ag_notifications(void)
{
    printf("AG notifications enabled \n");
    ag_sub_params.notify = ag_notify_cb;
    ag_sub_params.value_handle = ag_value_handle;
    ag_sub_params.ccc_handle = ag_notify_cccd_handle;
    ag_sub_params.value = BT_GATT_CCC_NOTIFY;
    bt_gatt_subscribe(default_conn, &ag_sub_params);
    ag_data_ready = true;
}


void read_ext_flash_raw(void)
{
    const struct device *flash_dev = DEVICE_DT_GET(DT_NODELABEL(rram_controller));
    uint8_t buf[INITIAL_READ_SIZE];
    if (!device_is_ready(flash_dev)) {
        printf("Flash device not ready\n");
        return;
    }
    int ret = flash_read(flash_dev, FW_START_ADDRESS, buf, INITIAL_READ_SIZE);
    if (ret) {
        printf("flash_read failed (%d)\n", ret);
    }
    // printf("data read from flash -------------------------------\r\n ");
    // for(int i=0; i<INITIAL_READ_SIZE;i++)
    // {
    //     printf ("%02x ", buf[i]);
    // }
    // printf("\r\n");
}

void my_flash_read(uint8_t *start_ptr, int size)
{
    const struct device *flash_dev = DEVICE_DT_GET(DT_NODELABEL(rram_controller));
    uint8_t buf[size];
    if (!device_is_ready(flash_dev)) {
        printf("Flash device not ready\n");
        return;
    }
    int ret = flash_read(flash_dev, start_ptr, buf, size);
    if (ret) {
        printf("flash_read failed (%d)\n", ret);
    }
    // printf("data read from flash -------------------------------\r\n ");
    // for(int i=0; i<INITIAL_READ_SIZE;i++)
    // {
    //     printf ("%02x ", buf[i]);
    // }
    // printf("\r\n");
}



void my_write_fun(uint8_t uuid[], uint8_t *data, int size){
    static int data_pack_no= 0;
    // memcpy(tx_data, data, size);
    if (memcmp(uuid, &ota_cmd_char_uuid.val, 16) == 0)
    {
        printf("value_handle == ota_cmd   size : %d\r\n", size); 
        k_sleep(K_MSEC(1000)); 
        int err = bt_gatt_write_without_response(default_conn, ota_cmd_value_handle, data, size, false);
        if (err) {
            printf("bt_gatt_write_without_response failed (err %d)\n", err);
        } else {
            printf("Data sent successfully\n");
        }
        k_sleep(K_MSEC(2000)); 
    }
    if (memcmp(uuid, &ota_data_char_uuid.val, 16) == 0)
    {
        printf("value_handle == ota_data   size : %d\r\n", size); 
        int err = bt_gatt_write_without_response(default_conn, ota_data_value_handle, data, size, false);
        if (err) {
            printf("bt_gatt_write_without_response failed (err %d)\n", err);
        } else {
            printf("Data sent successfully\n");
        }
    }

    printf("data_pack_no-> %d \r\ndata send-> ",data_pack_no);
    int k = 0;
    if(size > INITIAL_READ_SIZE ){
        k= INITIAL_READ_SIZE;
    }
    else {
        k= size;
    }
    for(int i=0; i<k;i++)
    {
        printf ("%02x ", data[i]);      // startting 10 bytes of every packet
    }
    printf("\r\n");
    k_sleep(K_MSEC(50)); 
    data_pack_no++;
}


// void service_def(void){
//     memcpy(ota_service.val, ota_service_uuid, 16);
//     ota_service.uuid.type = BT_UUID_TYPE_128;

//     memcpy(ota_cmd_char_uuid.val, ota_cmd_uuid, 16);
//     ota_cmd_char_uuid.uuid.type = BT_UUID_TYPE_128;

//     memcpy(ota_data_char_uuid.val, ota_data_uuid, 16);
//     ota_data_char_uuid.uuid.type = BT_UUID_TYPE_128;

//     memcpy(ag_service.val, ag_service_uuid, 16);
//     ag_service.uuid.type = BT_UUID_TYPE_128;

//     memcpy(ota_service.val, ag_data_uuid, 16);
//     ag_data_char_uuid.uuid.type = BT_UUID_TYPE_128;
// }


// void local_ota(void){
//     printf("Sending local ota...\n");
    
//     printf("mtu size ...\n");
//     uint8_t cmd[2] = {0xf4,0x00};//222
//     int err = bt_gatt_write_without_response(default_conn, ota_data_value_handle, cmd, 2, false);
//     if (err) {
//         printf("bt_gatt_write_without_response failed (err %d)\n", err);
//     } else {
//         printf("Data sent successfully\n");
//     }

//     printf("start ota ...\n");
//     uint8_t cmd1[1] = {0x01};
//     err = bt_gatt_write_without_response(default_conn, ota_cmd_value_handle, cmd1, 1, false);
//     if (err) {
//         printf("bt_gatt_write_without_response failed (err %d)\n", err);
//     } else {
//         printf("Data sent successfully\n");
//     }
// }

int main(void)
{
    int err,i = 0;
    // service_def();
    init_target_addr();
    start_scan();
	read_ext_flash_raw();
    while (1){
        if (ag_data_ready == true) {
                printf("Sending data...\n");
                err = bt_gatt_write_without_response(default_conn, ag_value_handle, tx_data, sizeof(tx_data), false);
                if (err) {
                    printf("bt_gatt_write_without_response failed (err %d)\n", err);
                } else {
                    printf("Data sent successfully\n");
                }
        }
        if(ota_ready == true && ota_done == false){
            printf("OTA is ready...\n");
            k_sleep(K_MSEC(2000)); 
            start_ota(MTU_SIZE,(uint8_t *)FW_START_ADDRESS,FW_SIZE, my_write_fun, my_flash_read);
            ota_done = true;
            printf("OTA update complete..\n");

        }
        k_sleep(K_MSEC(5000)); 
        printf ("while is running %10d \r\n", i++ );
   }
    return 0;
}