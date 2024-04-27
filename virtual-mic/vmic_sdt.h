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


#ifndef __VMIC_SDT_SUPPORT__
#define __VMIC_SDT_SUPPORT__

#include <stdint.h>
#include <stdbool.h>
#include <xrsr.h>
#include <xr_timestamp.h>
#include <jansson.h>

/// @file vmic_sdt.h
///
/// @defgroup VMIC SDT SPEECH REQUEST HANDLER
/// @{
///
/// @defgroup VMIC_DEFINITIONS Definitions
/// @defgroup VMIC_ENUMS       Enumerations
/// @defgroup VMIC_STRUCTS     Structures
/// @defgroup VMIC_TYPEDEFS    Type Definitions
/// @defgroup VMIC_FUNCTIONS   Functions
///

/// @addtogroup VMIC_DEFINITIONS
/// @{
/// @brief Macros for constant values
/// @details The VREX speech request handler provides macros for some parameters which may change in the future.  User applications should use
/// these names to allow the application code to function correctly if the values change.

#define VMIC_SDT_SESSION_ID_LEN_MAX      (64)  ///< Session identifier maximum length including NULL termination
#define VMIC_SDT_SESSION_STR_LEN_MAX     (512) ///< Session strings maximum length including NULL termination

/// @}
/// @addtogroup ENUMS
/// @{
/// @brief Enumerated Types
/// @details The VREX speech request handler provides enumerated types for logical groups of values.

/// @brief result types
/// @details The result enumeration indicates all the possible return codes.
/// @addtogroup VMIC_STRUCTS
/// @{
/// @brief Structures
/// @details The VREX speech request handler provides structures for grouping of values.

/// @brief VMIC param structure
/// @details The param data structure is used to provide input parameters to the vmic_sdt_open() function.  All string parameters must be NULL-terminated.  If a string parameter is not present, NULL must be set for it.
typedef struct {
   bool        test_flag;        ///< True if the device is used for testing only, otherwise false
   bool        mask_pii;         ///< True if the PII must be masked from the log
   void       *user_data;        ///< User data that is passed in to all of the callbacks
} vmic_sdt_params_t;

/// @brief VMIC stream parameter structure
/// @details The stream parameter data structure is returned in the session begin callback function.
typedef struct {
   uint32_t keyword_sample_begin;               ///< The offset in samples from the beginning of the buffered audio to the beginning of the keyword
   uint32_t keyword_sample_end;                 ///< The offset in samples from the beginning of the buffered audio to the end of the keyword
   uint16_t keyword_doa;                        ///< The direction of arrival in degrees (0-359)
   uint16_t keyword_sensitivity;                ///<
   uint16_t keyword_sensitivity_triggered;      ///<
   bool     keyword_sensitivity_high_support;   ///<
   bool     keyword_sensitivity_high_triggered; ///<
   uint16_t keyword_sensitivity_high;           ///<
   double   dynamic_gain;                       ///<
   double   signal_noise_ratio;                 ///<
   double   linear_confidence;                  ///<
   int32_t  nonlinear_confidence;               ///<
   bool     push_to_talk;                       ///< True if the session was started by the user pressing a button
} vmic_sdt_stream_params_t;

/// @}

/// @addtogroup VMIC_TYPEDEFS
/// @{
/// @brief Type Definitions
/// @details The VREX speech request handler provides type definitions for renaming types.

/// @brief vmic object type
/// @details The vmic object type is returned by the vmic_open api.  It is used in all subsequent calls to vmic api's.
typedef void * vmic_sdt_object_t;

/// @brief VMIC session begin handler
/// @details Function type to handle session begin events from the speech router.
/// @param[in] uuid          the session's unique identifier
/// @param[in] src           the input source for the audio stream
/// @param[in] configuration session configuration from the speech router
/// @param[in] stream_params parameters which describe the stream
/// @param[in] user_data     the data set by the user
/// @return The function has no return value.
typedef void (*vmic_sdt_handler_session_begin_t)(const uuid_t uuid, xrsr_src_t src, uint32_t dst_index, xrsr_session_config_out_t *config_out, vmic_sdt_stream_params_t *stream_params, rdkx_timestamp_t *timestamp, void *user_data);

/// @brief VMIC session end handler
/// @details Function type to handle session end events from the speech router.
/// @param[in] uuid  the session's unique identifier
/// @param[in] stats sesssion statistics from the speech router
/// @param[in] user_data     the data set by the user
/// @param[in] user_data     the data set by the user
/// @return The function has no return value.
typedef void (*vmic_sdt_handler_session_end_t)(const uuid_t uuid, xrsr_session_stats_t *stats, rdkx_timestamp_t *timestamp, void *user_data);

/// @brief VMIC stream begin handler
/// @details Function type to handle stream begin events from the speech router.
/// @param[in] uuid the session's unique identifier
/// @param[in] src  the input source for the error
/// @param[in] user_data     the data set by the user
/// @return The function has no return value.
typedef void (*vmic_sdt_handler_stream_begin_t)(const uuid_t uuid, xrsr_src_t src, rdkx_timestamp_t *timestamp, void *user_data);

/// @brief VMIC stream kwd handler
/// @details Function type to handle stream kwd events from the speech router.
/// @param[in] uuid the session's unique identifier
/// @param[in] user_data     the data set by the user
/// @return The function has no return value.
typedef void (*vmic_sdt_handler_stream_kwd_t)(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data);

/// @brief VMIC stream end handler
/// @details Function type to handle stream end events from the speech router.
/// @param[in] uuid  the session's unique identifier
/// @param[in] stats stream statistics from the speech router
/// @param[in] user_data     the data set by the user
/// @return The function has no return value.
typedef void (*vmic_sdt_handler_stream_end_t)(const uuid_t uuid, xrsr_stream_stats_t *stats, rdkx_timestamp_t *timestamp, void *user_data);

/// @brief VMIC stream connect handler
/// @details Function type to handle stream connect events from the speech router.
/// @param[in] uuid  the session's unique identifier
/// @param[in] user_data     the data set by the user
/// @return The function has no return value.
typedef void (*vmic_sdt_handler_connected_t)(const uuid_t uuid, rdkx_timestamp_t *timestamp, void *user_data);

/// @brief VMIC stream disconnect handler
/// @details Function type to handle stream disconnect events from the speech router.
/// @param[in] uuid  the session's unique identifier
/// @param[in] user_data     the data set by the user
/// @return The function has no return value.
typedef void (*vmic_sdt_handler_disconnected_t)(const uuid_t uuid, bool retry, rdkx_timestamp_t *timestamp, void *user_data);


/// @addtogroup VMIC_SDT_STRUCTS
/// @{

/// @brief VMIC SDT handlers structure
/// @details The handlers data structure is used to set all callback handlers for the vrex speech handler.  If a handler is not needed, it must be set to NULL.
typedef struct {
   vmic_sdt_handler_session_begin_t     session_begin;     ///< Indicates that a voice session has started
   vmic_sdt_handler_session_end_t       session_end;       ///< Indicates that a voice session has ended
   vmic_sdt_handler_stream_begin_t      stream_begin;      ///< An audio stream has started
   vmic_sdt_handler_stream_kwd_t        stream_kwd;        ///< The keyword has been passed in the stream
   vmic_sdt_handler_stream_end_t        stream_end;        ///< An audio stream has ended
   vmic_sdt_handler_connected_t         connected;         ///< The session has connected
   vmic_sdt_handler_disconnected_t      disconnected;      ///< The session has disconnected
} vmic_sdt_handlers_t;

/// @}

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup VMIC_FUNCTIONS
/// @{
/// @brief Function definitions
/// @details The VREX speech request handler provides functions to be called directly by the user application.

/// @brief Open the vrex speech request handler
/// @details Function which opens the vrex speech request handler interface.
/// @param[in] params Pointer to a structure of params which will be used while opening the interace.  Many of these parameters can be updated while the interface is open.
/// @return The function returns true for success, otherwise false.
vmic_sdt_object_t vmic_sdt_create(const vmic_sdt_params_t *params);


/// @brief Update the vmic speech request handler's PII mask
/// @details Function used to update the mask PII option.
/// @param[in] enable Masks PII if not false;
/// @return The function returns true for success, otherwise false.
bool vmic_sdt_update_mask_pii(vmic_sdt_object_t object, bool enable);

/// @brief Set the vrex speech request handlers
/// @details Function type to set the handlers for a given protocol.
/// @param[in] handlers_in  Set of handlers that are set by the application.
/// @param[in] handlers_out Set of handlers provided by this component for use by the application.
/// @return The function returns true for success, otherwise false.
bool vmic_sdt_handlers(vmic_sdt_object_t object, const vmic_sdt_handlers_t *handlers_in, xrsr_handlers_t *handlers_out);


/// @brief Close the vrex speech request handler
/// @details Function used to close the vrex speech request interface.
/// @return The function has no return value.
void vmic_sdt_destroy(vmic_sdt_object_t object);

/// @}

#ifdef __cplusplus
}
#endif

/// @}

#endif

