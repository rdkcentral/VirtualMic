/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef __VMIC_SDT_PRIVATE__
#define __VMIC_SDT_PRIVATE__

#include <rdkx_logger.h>
#include "vmic_sdt.h"

typedef struct {
   uint32_t             identifier;
   vmic_sdt_handlers_t  handlers;
   xrsr_handler_send_t  send;
   void *               param;
   bool                 mask_pii;
   void *               user_data;
} vmic_sdt_obj_t;

#endif

