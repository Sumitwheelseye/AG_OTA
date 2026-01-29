/**
 * @file    agota.h
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


#ifndef AGOTA_H
#define AGOTA_H

#include <stdint.h>

extern uint8_t ota_service_uuid[16];
extern uint8_t ota_cmd_uuid[16];
extern uint8_t ota_data_uuid[16];
extern uint8_t ag_service_uuid[16];
extern uint8_t ag_data_uuid[16];
extern uint8_t response[30];

void start_ota(int mtu_size, uint8_t *fw_start_ptr, int fw_size, 
        void (*f1ptr)( uint8_t uuid[],uint8_t data[], int size), 
        void (*f2ptr)(uint8_t data[], int size));
#endif