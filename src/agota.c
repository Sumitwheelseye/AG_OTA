/**
 * @file    agota.c
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


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <zephyr/kernel.h>
#include "agota.h"
uint16_t max_pack_size = 0;
int fw_chunk_no =0;
uint8_t *file = NULL;
uint8_t send_data[600] ={0}; 

uint8_t ota_service_uuid[16] = { 0xd8, 0xe6, 0xfd, 0x1d, 0x4a, 0x14, 0xc6, 0xb1, 0x53, 0x4c, 0x4c, 0x59, 0x6d, 0xd9, 0xf1, 0xd6 };
uint8_t ota_cmd_uuid[16]     = { 0x30, 0xd8, 0xe3, 0x3a, 0x0e, 0x27, 0x22, 0xb7, 0xa4, 0x46, 0xc0, 0x21, 0xaa, 0x71, 0xd6, 0x7a };
uint8_t ota_data_uuid[16]    = { 0xb0, 0xa5, 0xf8, 0x45, 0x8d, 0xca, 0x89, 0x9b, 0xd8, 0x4c, 0x40, 0x1f, 0x88, 0x88, 0x40, 0x23 };
uint8_t ag_service_uuid[16]  = { 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00 };
uint8_t ag_data_uuid[16]     = { 0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00 };

uint8_t response[30] = {0};

void (*ble_write)(uint8_t uuid[], uint8_t data[], int size);
void (*flash_read)(uint8_t *start_ptr, int size);

static bool start_cmd();
static void send_chunk(uint8_t *file_ptr , uint32_t file_size);
static bool complete_cmd();

void start_ota(int mtu_size, uint8_t *fw_start_ptr, int fw_size, 
                void (*f1ptr)(uint8_t uuid[],uint8_t data[], int size), 
                void (*f2ptr)(uint8_t data[], int size)){
    
    ble_write = f1ptr;
    flash_read = f2ptr;
    max_pack_size= mtu_size-3;
    memcpy(send_data, &max_pack_size, sizeof(uint16_t));

    (*ble_write)(ota_data_uuid,send_data, sizeof(uint16_t));// without response 
    printf("agota_init MTU size : %d packet size is : %d \r\n", mtu_size, max_pack_size);

    int result = start_cmd();
    if(result == false){
        printf ("Error while ota started\r\n ");
        return;
    }
    printf ("ota started succesfully %d \r\n",result);
    send_chunk(fw_start_ptr, fw_size);
    printf ("all chunk send to target device\r\n ");
    result=complete_cmd();
    if(result == false){
        printf ("Error while ota complete\r\n ");
        return;
    }
    printf ("ota complete succesfully %d\r\n",result);

}

static bool start_cmd() {
    memset(response, 0, sizeof(response));
    uint8_t cmd[1] = {0x01};
    (*ble_write)( ota_cmd_uuid ,cmd, sizeof(uint8_t));  //wait for response
    printf("start_agota data %d size%d \r\n", 0x01, sizeof(uint8_t));
    while(response[0] == 0x00){
        k_sleep(K_MSEC(10)); 
    }
    if (response[0] == 0x02 ){
        return true;
    }
    if (response[0] == 0x03 )
    {
        return false;
    }
    return false;
}

static void send_chunk(uint8_t *fw_start_ptr, uint32_t fw_size)
{
    uint32_t offset = 0;
    uint32_t chunk_size = 0;
    uint32_t fw_chunk_no = 0;
    printf("FW start ptr: %p, FW size: %d bytes\r\n",fw_start_ptr, fw_size);
    while (offset < fw_size)
    {
        if ((fw_size - offset) >= max_pack_size){
            chunk_size = max_pack_size;
        }
        else{
            chunk_size = fw_size - offset;   // last packet
        }
        uint8_t *chunk_ptr = fw_start_ptr + offset;
        printf("Chunk %u -> ptr: %p size: %u\r\n",fw_chunk_no, chunk_ptr, chunk_size);
        /* Read data from flash */
        (*flash_read)(chunk_ptr, chunk_size);
        /* Send over BLE */
        (*ble_write)(ota_data_uuid, chunk_ptr, chunk_size);
        offset += chunk_size;
        fw_chunk_no++;
    }
    printf("Firmware transfer completed. Total chunks: %u\r\n",fw_chunk_no);
}


static bool complete_cmd() {
    memset(response, 0, sizeof(response));
    uint8_t cmd[1] = {0x04};
    (*ble_write)( ota_cmd_uuid , cmd, 1);
    while(response[0] == 0x00){
        k_sleep(K_MSEC(10));
    }
    if (response[0] == 0x05 ){
        return true;
    }
    if (response[0] == 0x06 ){
        return false;
    }
    return false;
}
