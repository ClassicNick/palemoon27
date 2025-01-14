/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootCertificateTelemetryUtils.h"

#include "prlog.h"
#include "ScopedNSSTypes.h"
#include "mozilla/ArrayUtils.h"

// Note: New CAs will show up as UNKNOWN_ROOT until
// RootHashes.inc is updated to include them. 0 is reserved by
// genRootCAHashes.js for the unknowns.
#define UNKNOWN_ROOT  0
#define HASH_FAILURE -1

namespace mozilla { namespace psm { 

#if defined(PR_LOGGING)
PRLogModuleInfo* gPublicKeyPinningTelemetryLog =
  PR_NewLogModule("PublicKeyPinningTelemetryService");
#endif

// Used in the BinarySearch method, this does a memcmp between the pointer
// provided to its construtor and whatever the binary search is looking for.
//
// This implementation assumes everything to be of HASH_LEN, so it should not
// be used generically.
class BinaryHashSearchArrayComparator
{
public:
  explicit BinaryHashSearchArrayComparator(const uint8_t* aTarget, size_t len)
    : mTarget(aTarget)
  {
    NS_ASSERTION(len == HASH_LEN, "Hashes should be of the same length.");
  }

private:
  const uint8_t* mTarget;
};

// Perform a hash of the provided cert, then search in the RootHashes.inc data
// structure for a matching bin number.
int32_t
RootCABinNumber(const SECItem* cert)
{
  Digest digest;

  // Compute SHA256 hash of the certificate
  nsresult rv = digest.DigestBuf(SEC_OID_SHA256, cert->data, cert->len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return HASH_FAILURE;
  }

  // Compare against list of stored hashes
  size_t idx;

  PR_LOG(gPublicKeyPinningTelemetryLog, PR_LOG_DEBUG,
           ("pkpinTelem: First bytes %02hx %02hx %02hx %02hx\n",
            digest.get().data[0], digest.get().data[1], digest.get().data[2], digest.get().data[3]));

  // Didn't match.
  return UNKNOWN_ROOT;
}


// Attempt to increment the appropriate bin in the provided Telemetry probe ID. If
// there was a hash failure, we do nothing.
void
AccumulateTelemetryForRootCA(mozilla::Telemetry::ID probe, 
  const CERTCertificate* cert)
{
  int32_t binId = RootCABinNumber(&cert->derCert);

  if (binId != HASH_FAILURE) {
    Accumulate(probe, binId);
  }
}

} // namespace psm
} // namespace mozilla