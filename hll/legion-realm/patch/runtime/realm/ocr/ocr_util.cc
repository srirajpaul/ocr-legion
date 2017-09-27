/* Copyright 2017 Rice University, Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if USE_OCR_LAYER

#include <assert.h>
#include "ocr_util.h"
#include "extensions/ocr-affinity.h"
#include "extensions/ocr-legacy.h"
#include "extensions/ocr-runtime-itf.h"
#ifdef ENABLE_EXTENSION_LEGACY_FIBERS
#include "extensions/ocr-legacy-fibers.h"
#endif

namespace Realm {
  /*static*/ ocrHint_t * OCRUtil::ocrHintArr = NULL;

  /*static*/ void OCRUtil::static_init(ocrGuid_t* evts) {
    //create a table of hints indexed using their policy domain rank
    assert(ocrHintArr == NULL);
    int numPD = OCRUtil::ocrNbPolicyDomains();
    ocrHintArr = new ocrHint_t[numPD];
    ocrGuid_t myAffinity;
    ocrHint_t myHNT;
    ocrHintInit(&myHNT, OCR_HINT_EDT_T);
    for(int i=0; i<numPD; i++) {
      ocrAffinityGetAt(AFFINITY_PD, i, &(myAffinity));
      ocrSetHintValue(&myHNT, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(myAffinity));
      ocrHintArr[i] = myHNT;
    }
    //save the latch events to ELS for the two barriers
    if(evts != NULL) {
      ocrElsUserSet(0, evts[0]);
      ocrElsUserSet(1, evts[1]);
    }
  }

  /*static*/ void OCRUtil::static_destroy() {
    delete [] ocrHintArr;
    ocrHintArr = NULL;
  }
  
  void ocrBarrierHelper(int id) {
    if(OCRUtil::ocrNbPolicyDomains() == 1) return;

    //get the latch event from ELS, satisfy it and wait on the sticky event.
    //the latch event is triggered when it is satisfied by all the PD's
    ocrGuid_t latch_evt = ocrElsUserGet(id), sticky_evt;
    ocrEventCreate(&sticky_evt, OCR_EVENT_STICKY_T, 0);
    ocrAddDependence(latch_evt, sticky_evt, 0, DB_MODE_RO);
    //ocrAddDependence(NULL_GUID, latch_evt, OCR_EVENT_LATCH_DECR_SLOT, DB_MODE_RO);
    ocrEventSatisfySlot(latch_evt, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    OCRUtil::ocrLegacyBlock(sticky_evt);
  }
};

#ifdef USE_OCR_GASNET
#ifndef GASNET_PAR
#define GASNET_PAR
#endif
#include <gasnet.h>

namespace Realm {

/*static*/ u64 OCRUtil::ocrNbPolicyDomains() {
  return gasnet_nodes();
}

/*static*/ u64 OCRUtil::ocrCurrentPolicyDomain() {
  return gasnet_mynode();
}

/*static*/ void  OCRUtil::ocrBarrier(int id) {
  ocrBarrierHelper(id);

  //gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  //gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);
}

/*static*/ void OCRUtil::ocrLegacyBlock(ocrGuid_t dep) {
  ocrLegacyBlockProgress(dep, NULL, NULL, NULL, LEGACY_PROP_NONE);
}

};

#elif defined USE_OCR_MPI

#include <mpi.h>

namespace Realm {

/*static*/ u64 OCRUtil::ocrNbPolicyDomains() {
  int comm_size;
  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  return comm_size;
}

/*static*/ u64 OCRUtil::ocrCurrentPolicyDomain() {
  int my_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  return my_rank;
}

/*static*/ void  OCRUtil::ocrBarrier(int id) {
  ocrBarrierHelper(id);
  
  //To use MPI_Barrier, inside OCR runtime use MPI_Init_thread 
  //with MPI_THREAD_MULTIPLE instead of MPI_Init
  //MPI_Barrier(MPI_COMM_WORLD);
}

/*static*/ void OCRUtil::ocrLegacyBlock(ocrGuid_t dep) {
  ocrLegacyBlockProgress(dep, NULL, NULL, NULL, LEGACY_PROP_NONE);
}

};

#else // USE_OCR_GASNET_MPI

namespace Realm {

/*static*/ u64 OCRUtil::ocrNbPolicyDomains() {
  return 1;
}

/*static*/ u64 OCRUtil::ocrCurrentPolicyDomain() {
  return 0;
}

/*static*/ void OCRUtil::ocrBarrier(int id) {
}

/*static*/ void OCRUtil::ocrLegacyBlock(ocrGuid_t dep) {
#ifdef ENABLE_EXTENSION_LEGACY_FIBERS
  ocrLegacyFiberSuspendOnEvent(dep, DB_MODE_RO);
#else
  ocrLegacyBlockProgress(dep, NULL, NULL, NULL, LEGACY_PROP_NONE);
#endif
}

};

#endif // USE_OCR_GASNET_MPI

#endif // USE_OCR_LAYER

