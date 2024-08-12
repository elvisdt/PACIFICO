/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef H_BLECENT_
#define H_BLECENT_

#include "modlog/modlog.h"
#include "esp_central.h"
#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

#define BLECENT_SVC_ALERT_UUID              0xffb0
#define BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID  0xffb0
#define BLECENT_CHR_NEW_ALERT               0xffb1
#define BLECENT_CHR_SUP_UNR_ALERT_CAT_UUID  0xffb2
#define BLECENT_CHR_UNR_ALERT_STAT_UUID     0xffb3
// #define BLECENT_CHR_ALERT_NOT_CTRL_PT       0xffb0

#ifdef __cplusplus
}
#endif

#endif
