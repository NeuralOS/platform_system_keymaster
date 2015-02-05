/*
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hmac_operation.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace keymaster {

HmacOperation::HmacOperation(keymaster_purpose_t purpose, const Logger& logger,
                             const uint8_t* key_data, size_t key_data_size,
                             keymaster_digest_t digest, size_t tag_length)
    : Operation(purpose, logger), error_(KM_ERROR_OK), tag_length_(tag_length) {
    // Initialize CTX first, so dtor won't crash even if we error out later.
    HMAC_CTX_init(&ctx_);

    const EVP_MD* md;
    switch (digest) {
    case KM_DIGEST_SHA_2_224:
        md = EVP_sha224();
        break;
    case KM_DIGEST_SHA_2_256:
        md = EVP_sha256();
        break;
    case KM_DIGEST_SHA_2_384:
        md = EVP_sha384();
        break;
    case KM_DIGEST_SHA_2_512:
        md = EVP_sha512();
        break;
    default:
        error_ = KM_ERROR_UNSUPPORTED_DIGEST;
        return;
    }

    if ((int)tag_length_ > EVP_MD_size(md)) {
        error_ = KM_ERROR_UNSUPPORTED_MAC_LENGTH;
        return;
    }

    HMAC_Init_ex(&ctx_, key_data, key_data_size, md, NULL /* engine */);
}

HmacOperation::~HmacOperation() {
    HMAC_CTX_cleanup(&ctx_);
}

keymaster_error_t HmacOperation::Begin() {
    return error_;
}

keymaster_error_t HmacOperation::Update(const Buffer& input, Buffer* /* output */,
                                        size_t* input_consumed) {
    if (!HMAC_Update(&ctx_, input.peek_read(), input.available_read()))
        return KM_ERROR_UNKNOWN_ERROR;
    *input_consumed = input.available_read();
    return KM_ERROR_OK;
}

keymaster_error_t HmacOperation::Abort() {
    return KM_ERROR_OK;
}

keymaster_error_t HmacOperation::Finish(const Buffer& signature, Buffer* output) {
    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    if (!HMAC_Final(&ctx_, digest, &digest_len))
        return KM_ERROR_UNKNOWN_ERROR;

    switch (purpose()) {
    case KM_PURPOSE_SIGN:
        output->reserve(tag_length_);
        output->write(digest, tag_length_);
        return KM_ERROR_OK;
    case KM_PURPOSE_VERIFY:
        if (signature.available_read() != tag_length_)
            return KM_ERROR_INVALID_INPUT_LENGTH;
        if (memcmp(signature.peek_read(), digest, tag_length_) != 0)
            return KM_ERROR_VERIFICATION_FAILED;
        return KM_ERROR_OK;
    default:
        return KM_ERROR_UNSUPPORTED_PURPOSE;
    }
}

}  // namespace keymaster
