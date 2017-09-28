/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

//gcc -DENABLE_EXTENSION_PARAMS_EVT -DENABLE_EXTENSION_CHANNEL_EVT -I${OCR_INSTALL}/include -L${OCR_INSTALL}/lib -locr_${OCR_TYPE} ocr-reservations.c

#if !defined(ENABLE_EXTENSION_PARAMS_EVT) || !defined(ENABLE_EXTENSION_CHANNEL_EVT)
#error ENABLE_EXTENSION_PARAMS_EVT and ENABLE_EXTENSION_CHANNEL_EVT should be defined
#else

#include "ocr.h"
#include "ocr-reservations.h"

/**
 * @brief Create a Reservation
 **/
u8 ocrReservationCreate(ocrGuid_t *res, void *in_params)
{
    ocrAssert(in_params == NULL); //for now
    //reservation is represented using a channel event
    ocrEventParams_t params;
    params.EVENT_CHANNEL.maxGen = RESERVATION_SIZE;
    params.EVENT_CHANNEL.nbSat = 1;
    params.EVENT_CHANNEL.nbDeps = 1;
    u8 ret = ocrEventCreateParams(res, OCR_EVENT_CHANNEL_T, false, &params);
    //This EventSatisfy will help to trigger the first acquire
    //by matching the add dependency done in aqcuire
    ocrEventSatisfy(*res, NULL_GUID);
    return ret;
}

/**
 * @brief Acquire a reservation
 **/
u8 ocrReservationAcquireRequest(ocrGuid_t res, ocrReservationMode_t mode, u32 depc, ocrGuid_t *depv, ocrGuid_t *outputEvent)
{
    u8 ret;
    //if no dependency, then add the output event directly to channel event
    if(depc == 0 || (depc == 1 && ocrGuidIsEq(NULL_GUID, depv[0]))) {
      ret = ocrAddDependence(res, *outputEvent, 0, DB_MODE_RO);
    }
    //for one dependency, create a latch event, add dependency and channel event to its
    //increment and decrement slots. attach the latch event to the output event
    else if (depc == 1) {
      ocrGuid_t latchEvent;
      ret = ocrEventCreate(&latchEvent, OCR_EVENT_LATCH_T, false);
      ret |= ocrAddDependence(latchEvent, *outputEvent, 0, DB_MODE_RO);
      ret |= ocrAddDependence(res, latchEvent, OCR_EVENT_LATCH_INCR_SLOT, DB_MODE_RO);
      ret |= ocrAddDependence(depv[0], latchEvent, OCR_EVENT_LATCH_DECR_SLOT, DB_MODE_RO);
    }
    //for more than one dependency, create a latch event, add channel event and dependencies
    //to the latch event. attach the latch event to the output event
    else {
      ocrEventParams_t params;
      params.EVENT_LATCH.counter = depc + 1;
      ocrGuid_t latchEvent; 
      ret = ocrEventCreateParams(&latchEvent, OCR_EVENT_LATCH_T, false, &params);
      ret |= ocrAddDependence(latchEvent, *outputEvent, 0, DB_MODE_RO);
      u32 i;
      ret |= ocrAddDependence(res, latchEvent, OCR_EVENT_LATCH_DECR_SLOT, DB_MODE_RO);
      for(i=0; i<depc; i++)
          ret |= ocrAddDependence(depv[i], latchEvent, OCR_EVENT_LATCH_DECR_SLOT, DB_MODE_RO);
    }
    return ret;
}

/**
 * @brief Release a reservation
 *
 * Releasing a reservation triggers the next pending acquire
 **/
u8 ocrReservationReleaseRequest(ocrGuid_t res, u32 depc, ocrGuid_t *depv)
{
    u8 ret;
    //if no dependency, directly satify the channel event
    if(depc == 0 || (depc == 1 && ocrGuidIsEq(NULL_GUID, depv[0]))) {
      ret = ocrEventSatisfy(res, NULL_GUID);
    }
    //for one dependency, satisfy the channel event with that dependency
    else if(depc == 1) {
      ret = ocrAddDependence(depv[0], res, 0, DB_MODE_RO);
    }
    //for more than one dependency, attach the dependencies to a latch event
    //and use the latch event to satisfy the channel event
    else {
      ocrEventParams_t params;
      params.EVENT_LATCH.counter = depc;
      ocrGuid_t latchEvent;
      ret = ocrEventCreateParams(&latchEvent, OCR_EVENT_LATCH_T, false, &params);
      u32 i;

      ret != ocrAddDependence(latchEvent, res, 0, DB_MODE_RO);
      for(i=0; i<depc; i++)
          ret |= ocrAddDependence(depv[i], latchEvent, OCR_EVENT_LATCH_DECR_SLOT, DB_MODE_RO);
    }
    return ret;
}

/**
 * @brief Destroys a reservation
 **/
u8 ocrReservationDestroy(ocrGuid_t res)
{
    return ocrEventDestroy(res);
}

#endif
