/*
 * COPYRIGHT (c) International Business Machines Corp. 2005-2017
 *
 * This program is provided under the terms of the Common Public License,
 * version 1.0 (CPL-1.0). Any use, reproduction or distribution for this
 * software constitutes recipient's acceptance of CPL-1.0 terms which can be
 * found in the file LICENSE file or at
 * https://opensource.org/licenses/cpl1.0.php
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "pkcs11types.h"
#include "regress.h"
#include "des.h"
#include "common.c"
#include "mech_to_str.h"

/** Tests DES encryption with published test vectors. **/
CK_RV do_EncryptDES(struct published_test_suite_info *tsuite)
{
    unsigned int i;
    CK_BYTE expected[BIG_REQUEST];
    CK_BYTE actual[BIG_REQUEST];
    CK_ULONG expected_len, actual_len;

    CK_BYTE user_pin[PKCS11_MAX_PIN_LEN];
    CK_ULONG user_pin_len;
    CK_SESSION_HANDLE session;
    CK_MECHANISM mech;
    CK_OBJECT_HANDLE h_key;
    CK_RV rc;
    CK_FLAGS flags;
    CK_SLOT_ID slot_id = SLOT_ID;

    /** begin testsuite **/
    testsuite_begin("%s Encryption.", tsuite->name);
    testcase_rw_session();
    testcase_user_login();


    /** skip test if the slot doesn't support this mechanism **/
    if (!mech_supported(slot_id, tsuite->mechanism)) {
        testsuite_skip(tsuite->tvcount,
                       "Slot %u doesn't support %s (0x%x)",
                       (unsigned int) slot_id,
                       mech_to_str(tsuite->mechanism),
                       (unsigned int) tsuite->mechanism);
        goto testcase_cleanup;
    }

    /** iterate over test vectors **/
    for (i = 0; i < tsuite->tvcount; i++) {

        /** begin testcase **/
        testcase_begin("%s Encryption with published test vector "
                       "%d.", tsuite->name, i);

        rc = CKR_OK;            // set rc

        /** clear buffers **/
        memset(expected, 0, sizeof(expected));
        memset(actual, 0, sizeof(actual));

        /** get ciphertext (expected results) **/
        expected_len = tsuite->tv[i].clen;
        memcpy(expected, tsuite->tv[i].ciphertext, expected_len);

        /** get plaintext **/
        actual_len = tsuite->tv[i].plen;
        memcpy(actual, tsuite->tv[i].plaintext, actual_len);

        /** get mech **/
        mech.mechanism = tsuite->mechanism;
        mech.ulParameterLen = tsuite->tv[i].ivlen;
        mech.pParameter = tsuite->tv[i].iv;

        /** create key handle. **/
        rc = create_DESKey(session,
                           tsuite->tv[i].key, tsuite->tv[i].klen, &h_key);
        if (rc != CKR_OK) {
            if (rc == CKR_POLICY_VIOLATION) {
                testcase_skip("DES key import is not allowed by policy");
                continue;
            }

            testcase_error("C_CreateObject rc=%s", p11_get_ckr(rc));
            goto testcase_cleanup;
        }

        /** initialize single encryption **/
        rc = funcs->C_EncryptInit(session, &mech, h_key);
        if (rc != CKR_OK) {
            testcase_error("C_EncryptInit rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /** do single (in-place) encryption **/
        rc = funcs->C_Encrypt(session, actual, actual_len, actual, &actual_len);
        if (rc != CKR_OK) {
            testcase_error("C_Encrypt rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /** compare encryption results with expected results. **/
        testcase_new_assertion();

        if (actual_len != expected_len) {
            testcase_fail("encrypted data length does not "
                          "match test vector's encrypted data "
                          "length.\n\nexpected length=%ld, but found"
                          " length=%ld\n", expected_len, actual_len);
        } else if (memcmp(actual, expected, expected_len)) {
            testcase_fail("encrypted data does not match test "
                          "vector's encrypted data");
        } else {
            testcase_pass("%s Encryption with test vector %d "
                          "passed.", tsuite->name, i);
        }

        /** clean up **/
        rc = funcs->C_DestroyObject(session, h_key);
        if (rc != CKR_OK) {
            testcase_error("C_DestroyObject rc=%s", p11_get_ckr(rc));
        }

    }
    goto testcase_cleanup;

error:
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK)
        testcase_error("C_DestroyObject rc=%s.", p11_get_ckr(rc));

testcase_cleanup:
    testcase_user_logout();
    rc = funcs->C_CloseAllSessions(slot_id);
    if (rc != CKR_OK) {
        testcase_error("C_CloseAllSessions rc=%s", p11_get_ckr(rc));
    }

    return rc;
}

/** Tests DES multipart encryption with published test vectors. **/
CK_RV do_EncryptUpdateDES(struct published_test_suite_info * tsuite)
{
    unsigned int i;
    CK_BYTE expected[BIG_REQUEST];
    CK_BYTE plaintext[BIG_REQUEST];
    CK_BYTE crypt[BIG_REQUEST];
    CK_ULONG expected_len, p_len, crypt_len = 0, k;

    CK_BYTE user_pin[PKCS11_MAX_PIN_LEN];
    CK_ULONG user_pin_len;
    CK_SESSION_HANDLE session;
    CK_MECHANISM mech;
    CK_OBJECT_HANDLE h_key;
    CK_RV rc;
    CK_FLAGS flags;
    CK_SLOT_ID slot_id = SLOT_ID;

    /** begin testsuite **/
    testsuite_begin("%s Multipart Encryption.", tsuite->name);
    testcase_rw_session();
    testcase_user_login();

    /** skip test if slot doesn't support this mechanism **/
    if (!mech_supported(slot_id, tsuite->mechanism)) {
        testsuite_skip(tsuite->tvcount,
                       "Slot %u doesn't support %s (0x%x)",
                       (unsigned int) slot_id,
                       mech_to_str(tsuite->mechanism),
                       (unsigned int) tsuite->mechanism);
        goto testcase_cleanup;
    }

    /** iterate over test vectors **/
    for (i = 0; i < tsuite->tvcount; i++) {

        /** begin test **/
        testcase_begin("%s Multipart Encryption with published "
                       "test vector %d.", tsuite->name, i);

        rc = CKR_OK;            // set rc


        /** clear buffers **/
        memset(expected, 0, sizeof(expected));
        memset(crypt, 0, sizeof(crypt));
        memset(plaintext, 0, sizeof(plaintext));

        /** get ciphertext (expected results) **/
        expected_len = tsuite->tv[i].clen;
        memcpy(expected, tsuite->tv[i].ciphertext, expected_len);

        /** get plaintext **/
        p_len = tsuite->tv[i].plen;
        memcpy(plaintext, tsuite->tv[i].plaintext, p_len);

        /** get mech **/
        mech.mechanism = tsuite->mechanism;
        mech.ulParameterLen = tsuite->tv[i].ivlen;
        mech.pParameter = tsuite->tv[i].iv;

        /** create key handle **/
        rc = create_DESKey(session,
                           tsuite->tv[i].key, tsuite->tv[i].klen, &h_key);
        if (rc != CKR_OK) {
            if (rc == CKR_POLICY_VIOLATION) {
                testcase_skip("DES key import is not allowed by policy");
                continue;
            }

            testcase_error("C_CreateObject rc=%s", p11_get_ckr(rc));
            goto testcase_cleanup;
        }

        /** initialize multipart encryption **/
        rc = funcs->C_EncryptInit(session, &mech, h_key);
        if (rc != CKR_OK) {
            testcase_error("C_EncryptInit rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /* do multipart encryption
         * for chunks, -1 is NULL, and 0 is empty string,
         * and a value > 0 is amount of data from test vector's
         * plaintext data. The is way we test input in various sizes.
         */
        k = 0;
        if (tsuite->tv[i].num_chunks) {
            int j;
            CK_ULONG outlen, len;
            CK_BYTE *data_chunk = NULL;

            crypt_len = 0;
            outlen = sizeof(crypt);

            for (j = 0; j < tsuite->tv[i].num_chunks; j++) {
                if (tsuite->tv[i].chunks[j] == -1) {
                    len = 0;
                    data_chunk = NULL;
                } else if (tsuite->tv[i].chunks[j] == 0) {
                    len = 0;
                    data_chunk = (CK_BYTE *) "";
                } else {
                    len = tsuite->tv[i].chunks[j];
                    data_chunk = plaintext + k;
                }

                rc = funcs->C_EncryptUpdate(session, data_chunk,
                                            len, &crypt[crypt_len], &outlen);
                if (rc != CKR_OK) {
                    testcase_error("C_EncryptUpdate rc=%s", p11_get_ckr(rc));
                    goto error;
                }
                k += len;
                crypt_len += outlen;
                outlen = sizeof(crypt) - crypt_len;
            }
        } else {
            crypt_len = sizeof(crypt);
            rc = funcs->C_EncryptUpdate(session, plaintext, p_len,
                                        crypt, &crypt_len);
            if (rc != CKR_OK) {
                testcase_error("C_EncryptUpdate rc=%s", p11_get_ckr(rc));
                goto error;
            }
        }

        /** finalize multipart encryption **/
        k = sizeof(crypt) - crypt_len;
        rc = funcs->C_EncryptFinal(session, &crypt[p_len], &k);
        if (rc != CKR_OK) {
            testcase_error("C_EncryptFinal rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /** compare encryption results with expected results. **/
        testcase_new_assertion();

        if (crypt_len != expected_len) {
            testcase_fail("encrypted multipart data length "
                          "does not match test vector's encrypted "
                          "data length.\n\nexpected length=%ld, "
                          "but found length=%ld\n", expected_len, crypt_len);
        } else if (memcmp(crypt, expected, expected_len)) {
            testcase_fail("encrypted multipart data does not "
                          "match test vector's encrypted data.\n");
        } else {
            testcase_pass("%s Multipart Encryption with test "
                          "vector %d passed.", tsuite->name, i);
        }

        rc = funcs->C_DestroyObject(session, h_key);
        if (rc != CKR_OK) {
            testcase_error("C_DestroyObject rc=%s", p11_get_ckr(rc));
            goto testcase_cleanup;
        }
    }
    goto testcase_cleanup;

error:
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK)
        testcase_error("C_DestroyObject rc=%s.", p11_get_ckr(rc));

testcase_cleanup:
    testcase_user_logout();
    rc = funcs->C_CloseAllSessions(slot_id);
    if (rc != CKR_OK) {
        testcase_error("C_CloseAllSessions rc=%s", p11_get_ckr(rc));
    }

    return rc;
}

/** Tests DES decryption with published test vectors. **/
CK_RV do_DecryptDES(struct published_test_suite_info * tsuite)
{
    unsigned int i;
    CK_BYTE expected[BIG_REQUEST];
    CK_BYTE actual[BIG_REQUEST];
    CK_ULONG expected_len, actual_len;

    CK_BYTE user_pin[PKCS11_MAX_PIN_LEN];
    CK_ULONG user_pin_len;
    CK_SESSION_HANDLE session;
    CK_MECHANISM mech;
    CK_OBJECT_HANDLE h_key;
    CK_RV rc;
    CK_FLAGS flags;
    CK_SLOT_ID slot_id = SLOT_ID;

    /** begin testsuite **/
    testsuite_begin("%s Decryption.", tsuite->name);
    testcase_rw_session();
    testcase_user_login();

    rc = CKR_OK;

    /** query the slot to see if the mech is supported. **/
    if (!mech_supported(slot_id, tsuite->mechanism)) {
        testsuite_skip(tsuite->tvcount,
                       "Slot %u doesn't support %s (0x%x)",
                       (unsigned int) slot_id,
                       mech_to_str(tsuite->mechanism),
                       (unsigned int) tsuite->mechanism);
        goto testcase_cleanup;
    }

    /** iterate over test vector **/
    for (i = 0; i < tsuite->tvcount; i++) {

        /** begin test **/
        testcase_begin("%s Decryption with published test vector " "%d.",
                       tsuite->name, i);

        rc = CKR_OK;

        /** create key handle. **/
        rc = create_DESKey(session,
                           tsuite->tv[i].key, tsuite->tv[i].klen, &h_key);
        if (rc != CKR_OK) {
            if (rc == CKR_POLICY_VIOLATION) {
                testcase_skip("DES key import is not allowed by policy");
                continue;
            }

            testcase_error("C_CreateObject rc=%s", p11_get_ckr(rc));
            goto testcase_cleanup;
        }

        /** get mech **/
        mech.mechanism = tsuite->mechanism;
        mech.ulParameterLen = tsuite->tv[i].ivlen;
        mech.pParameter = tsuite->tv[i].iv;

        /** clear buffers **/
        memset(expected, 0, sizeof(expected));
        memset(actual, 0, sizeof(actual));

        /** get plaintext (expected results) **/
        expected_len = tsuite->tv[i].plen;
        memcpy(expected, tsuite->tv[i].plaintext, expected_len);

        /** get ciphertext **/
        actual_len = tsuite->tv[i].clen;
        memcpy(actual, tsuite->tv[i].ciphertext, actual_len);

        /** initialize single decryption **/
        rc = funcs->C_DecryptInit(session, &mech, h_key);
        if (rc != CKR_OK) {
            testcase_error("C_DecryptInit rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /** do single (in-place) decryption **/
        rc = funcs->C_Decrypt(session, actual, actual_len, actual, &actual_len);
        if (rc != CKR_OK) {
            testcase_error("C_Decrypt rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /** compare decryption results with expected results. **/
        testcase_new_assertion();

        if (actual_len != expected_len) {
            testcase_fail("decrypted data length does not "
                          "match test vector's decrypted data "
                          "length.\n\nexpected length=%ld, but found"
                          " length=%ld\n", expected_len, actual_len);
        } else if (memcmp(actual, expected, expected_len)) {
            testcase_fail("decrypted data does not match test "
                          "vector's decrypted data.\n");
        } else {
            testcase_pass("%s Decryption with test vector %d "
                          "passed.", tsuite->name, i);
        }

    }

    /** clean up **/
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_DestroyObject rc=%s", p11_get_ckr(rc));
    }
    goto testcase_cleanup;

error:
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK)
        testcase_error("C_DestroyObject rc=%s.", p11_get_ckr(rc));

testcase_cleanup:
    testcase_user_logout();
    rc = funcs->C_CloseAllSessions(slot_id);
    if (rc != CKR_OK) {
        testcase_error("C_CloseAllSessions rc=%s", p11_get_ckr(rc));
    }

    return rc;
}

/** Tests DES multipart decryption with published test vectors. **/
CK_RV do_DecryptUpdateDES(struct published_test_suite_info * tsuite)
{
    unsigned int i;
    CK_BYTE expected[BIG_REQUEST];
    CK_BYTE cipher[BIG_REQUEST];
    CK_BYTE plaintext[BIG_REQUEST];
    CK_ULONG expected_len, p_len, cipher_len, k;

    CK_BYTE user_pin[PKCS11_MAX_PIN_LEN];
    CK_ULONG user_pin_len;
    CK_SESSION_HANDLE session;
    CK_MECHANISM mech;
    CK_OBJECT_HANDLE h_key;
    CK_RV rc;
    CK_FLAGS flags;
    CK_SLOT_ID slot_id = SLOT_ID;

    /** begin testsuite **/
    testsuite_begin("%s Multipart Decryption.", tsuite->name);
    testcase_rw_session();
    testcase_user_login();

    rc = CKR_OK;                // set rc

    /** skip testsuite if the slot doesn't support this mechanism **/
    if (!mech_supported(slot_id, tsuite->mechanism)) {
        testsuite_skip(tsuite->tvcount,
                       "Slot %u doesn't support %s (0x%x)",
                       (unsigned int) slot_id,
                       mech_to_str(tsuite->mechanism),
                       (unsigned int) tsuite->mechanism);
        goto testcase_cleanup;
    }

    /** iterate over test vectors **/
    for (i = 0; i < tsuite->tvcount; i++) {

        /** begin test **/
        testcase_begin("%s Multipart Decryption with published "
                       "test vector %d.", tsuite->name, i);

        rc = CKR_OK;            // set rc

        /** clear buffers **/
        memset(expected, 0, sizeof(expected));
        memset(cipher, 0, sizeof(cipher));
        memset(plaintext, 0, sizeof(plaintext));

        /** get plaintext (expected results) **/
        expected_len = tsuite->tv[i].plen;
        memcpy(expected, tsuite->tv[i].plaintext, expected_len);

        /** get ciphertext **/
        cipher_len = tsuite->tv[i].clen;
        memcpy(cipher, tsuite->tv[i].ciphertext, cipher_len);

        /** get mech **/
        mech.mechanism = tsuite->mechanism;
        mech.ulParameterLen = tsuite->tv[i].ivlen;
        mech.pParameter = tsuite->tv[i].iv;

        /** create key handle. **/
        rc = create_DESKey(session,
                           tsuite->tv[i].key, tsuite->tv[i].klen, &h_key);
        if (rc != CKR_OK) {
            if (rc == CKR_POLICY_VIOLATION) {
                testcase_skip("DES key import is not allowed by policy");
                continue;
            }

            testcase_error("C_CreateObject rc=%s", p11_get_ckr(rc));
            goto testcase_cleanup;
        }

        /** initialize multipart decryption **/
        rc = funcs->C_DecryptInit(session, &mech, h_key);
        if (rc != CKR_OK) {
            testcase_error("C_DecryptInit rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /* do multipart encryption
         * for chunks, -1 is NULL, and 0 is empty string,
         * and a value > 0 is amount of data from test vector's
         * plaintext data. The is way we test input in various sizes.
         */
        k = 0;
        if (tsuite->tv[i].num_chunks) {
            int j;
            CK_ULONG outlen, len;
            CK_BYTE *data_chunk = NULL;

            p_len = 0;
            outlen = sizeof(plaintext);
            for (j = 0; j < tsuite->tv[i].num_chunks; j++) {
                if (tsuite->tv[i].chunks[j] == -1) {
                    len = 0;
                    data_chunk = NULL;
                } else if (tsuite->tv[i].chunks[j] == 0) {
                    len = 0;
                    data_chunk = (CK_BYTE *) "";
                } else {
                    len = tsuite->tv[i].chunks[j];
                    data_chunk = cipher + k;
                }

                rc = funcs->C_DecryptUpdate(session, data_chunk,
                                            len, &plaintext[p_len], &outlen);
                if (rc != CKR_OK) {
                    testcase_error("C_DecryptUpdate rc=%s", p11_get_ckr(rc));
                    goto error;
                }
                k += len;
                p_len += outlen;
                outlen = sizeof(plaintext) - p_len;
            }
        } else {
            p_len = sizeof(plaintext);
            rc = funcs->C_DecryptUpdate(session, cipher, cipher_len,
                                        plaintext, &p_len);
            if (rc != CKR_OK) {
                testcase_error("C_DecryptUpdate rc=%s", p11_get_ckr(rc));
                goto error;
            }
        }

        /** finalize multipart decryption **/
        k = sizeof(plaintext) - p_len;
        rc = funcs->C_DecryptFinal(session, &plaintext[k], &k);
        if (rc != CKR_OK) {
            testcase_error("C_DecryptFinal rc=%s", p11_get_ckr(rc));
            goto error;
        }

        /** compare decryption results with expected results. **/
        testcase_new_assertion();

        if (p_len != expected_len) {
            testcase_fail("decrypted multipart data length "
                          "does not  match test vector's decrypted "
                          "data length.\n\nexpected length=%ld, but "
                          "found length=%ld\n", expected_len, p_len);
        } else if (memcmp(plaintext, expected, expected_len)) {
            testcase_fail("decrypted multipart data does not "
                          "match test vector's decrypted data.\n");
        } else {
            testcase_pass("%s Multipart Decryption with test "
                          "vector  %d passed.", tsuite->name, i);
        }

        /** clean up **/
        rc = funcs->C_DestroyObject(session, h_key);
        if (rc != CKR_OK) {
            testcase_error("C_DestroyObject rc=%s", p11_get_ckr(rc));
            goto testcase_cleanup;
        }

    }
    goto testcase_cleanup;

error:
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK)
        testcase_error("C_DestroyObject rc=%s.", p11_get_ckr(rc));

testcase_cleanup:
    testcase_user_logout();
    rc = funcs->C_CloseAllSessions(slot_id);
    if (rc != CKR_OK) {
        testcase_error("C_CloseAllSessions rc=%s", p11_get_ckr(rc));
    }

    return rc;
}

/** Tests DES encryption & decryption with generated (secure) keys. **/
CK_RV do_EncryptDecryptDES(struct generated_test_suite_info * tsuite)
{
    unsigned int j;
    CK_BYTE original[BIG_REQUEST];
    CK_BYTE crypt[BIG_REQUEST + DES_BLOCK_SIZE];
    CK_BYTE decrypt[BIG_REQUEST + DES_BLOCK_SIZE];
    CK_ULONG original_len, crypt_len, decrypt_len;

    CK_SESSION_HANDLE session;
    CK_MECHANISM mech, mechkey;
    CK_OBJECT_HANDLE h_key;
    CK_FLAGS flags;
    CK_BYTE user_pin[PKCS11_MAX_PIN_LEN];
    CK_ULONG user_pin_len;
    CK_SLOT_ID slot_id = SLOT_ID;
    CK_RV rc;

    /** begin test **/
    testcase_begin("%s Encryption/Decryption with key generation "
                   "test.", tsuite->name);
    testcase_rw_session();
    testcase_user_login();

    /** skip test if the slot doesn't support this mechanism **/
    if (!mech_supported(slot_id, tsuite->mech.mechanism)) {
        testcase_skip("Slot %u doesn't support %s (0x%x)",
                      (unsigned int) slot_id,
                      mech_to_str(tsuite->mech.mechanism),
                      (unsigned int) tsuite->mech.mechanism);
        goto testcase_cleanup;
    }

    /** clear buffers **/
    memset(original, 0, sizeof(original));
    memset(crypt, 0, sizeof(crypt));
    memset(decrypt, 0, sizeof(decrypt));

    /** generate test data **/
    original_len = sizeof(original);
    crypt_len = sizeof(crypt);
    decrypt_len = sizeof(decrypt);

    for (j = 0; j < original_len; j++)
        original[j] = j % 255;

    /** set mechanism for key generation **/
    mechkey = des_keygen;

    /** generate key **/
    rc = funcs->C_GenerateKey(session, &mechkey, NULL, 0, &h_key);
    if (rc != CKR_OK) {
        testcase_error("C_GenerateKey rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** set mech for crypto **/
    mech = tsuite->mech;

    /** initialize single encryption **/
    rc = funcs->C_EncryptInit(session, &mech, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_EncryptInit rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** do single encryption **/
    rc = funcs->C_Encrypt(session, original, original_len, crypt, &crypt_len);
    if (rc != CKR_OK) {
        testcase_error("C_Encrypt rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** initialize single decryption **/
    rc = funcs->C_DecryptInit(session, &mech, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_DecryptInit rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** do single decryption **/
    rc = funcs->C_Decrypt(session, crypt, crypt_len, decrypt, &decrypt_len);
    if (rc != CKR_OK) {
        testcase_error("C_Decrypt rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** compare encryption/decryption results with original data **/
    testcase_new_assertion();

    if (decrypt_len != original_len) {
        testcase_fail("decrypted data length does not match "
                      "original data length.\nexpected length=%ld, "
                      "but found length=%ld\n", original_len, decrypt_len);
    } else if (memcmp(decrypt, original, original_len)) {
        testcase_fail("decrypted data does not match original " "data");
    } else {
        testcase_pass("%s Encryption/Decryption with key "
                      "generation test passed.", tsuite->name);
    }

    /** clean up **/
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_DestroyObject rc=%s", p11_get_ckr(rc));
    }
    goto testcase_cleanup;

error:
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK)
        testcase_error("C_DestroyObject rc=%s.", p11_get_ckr(rc));
    goto testcase_cleanup;

testcase_cleanup:
    testcase_user_logout();
    rc = funcs->C_CloseAllSessions(slot_id);
    if (rc != CKR_OK) {
        testcase_error("C_CloseAllSessions rc=%s", p11_get_ckr(rc));
    }

    return rc;
}

/** Tests DES multipart encryption & decryption with generated keys. **/
CK_RV do_EncryptDecryptUpdateDES(struct generated_test_suite_info * tsuite)
{
    unsigned int i, j, k;
    CK_BYTE original[BIG_REQUEST];
    CK_BYTE crypt[BIG_REQUEST + DES_BLOCK_SIZE];
    CK_BYTE decrypt[BIG_REQUEST + DES_BLOCK_SIZE];
    CK_ULONG original_len, crypt_len, decrypt_len;
    CK_ULONG tmp;

    CK_SESSION_HANDLE session;
    CK_MECHANISM mech, mechkey;
    CK_OBJECT_HANDLE h_key;
    CK_FLAGS flags;
    CK_BYTE user_pin[PKCS11_MAX_PIN_LEN];
    CK_ULONG user_pin_len;
    CK_SLOT_ID slot_id = SLOT_ID;
    CK_RV rc;

    /** begin test **/
    testcase_begin("%s Multipart Encryption/Decryption with key "
                   "generation test.", tsuite->name);
    testcase_rw_session();
    testcase_user_login();

    /** skip test if the slot doesn't support this mechanism **/
    if (!mech_supported(slot_id, tsuite->mech.mechanism)) {
        testcase_skip("Slot %u doesn't support %s (0x%x)",
                      (unsigned int) slot_id,
                      mech_to_str(tsuite->mech.mechanism),
                      (unsigned int) tsuite->mech.mechanism);
        goto testcase_cleanup;
    }

    /** clear buffers **/
    memset(original, 0, sizeof(original));
    memset(crypt, 0, sizeof(crypt));
    memset(decrypt, 0, sizeof(decrypt));

    /** generate test data **/
    original_len = sizeof(original);

    for (j = 0; j < original_len; j++)
        original[j] = j % 255;

    /** set mechanism for key gen **/
    mechkey = des_keygen;

    /** generate key **/
    rc = funcs->C_GenerateKey(session, &mechkey, NULL, 0, &h_key);
    if (rc != CKR_OK) {
        testcase_error("C_GenerateKey rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** set mech for crypto **/
    mech = tsuite->mech;

    /** initialize multipart encryption **/
    rc = funcs->C_EncryptInit(session, &mech, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_EncryptInit #1 rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** do multipart encryption **/
    i = k = 0;                  // i indexes source buffer
    // k indexes destination buffer
    crypt_len = sizeof(crypt);
    tmp = 0;

    while (i < original_len) {
        tmp = crypt_len - k;    // room left in mpcrypt
        rc = funcs->C_EncryptUpdate(session,
                                    &original[i],
                                    DES_BLOCK_SIZE, &crypt[k], &tmp);
        if (rc != CKR_OK) {
            testcase_error("C_EncryptUpdate rc=%s", p11_get_ckr(rc));
            goto error;
        }
        k += tmp;
        i += DES_BLOCK_SIZE;
    }

    /** finalize multipart encryption **/
    crypt_len = sizeof(crypt) - k;

    rc = funcs->C_EncryptFinal(session, &crypt[k], &crypt_len);
    if (rc != CKR_OK) {
        testcase_error("C_EncryptFinal rc=%s", p11_get_ckr(rc));
        goto error;
    }

    crypt_len += k;

    /** initialize multipart decryption **/
    rc = funcs->C_DecryptInit(session, &mech, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_DecryptInit rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** do multipart decryption **/
    i = k = 0;                  // i indexes source buffer
    // k indexes destination buffer
    decrypt_len = sizeof(decrypt);

    while (i < crypt_len) {
        tmp = decrypt_len - k;  // room left in mpdecrypt
        rc = funcs->C_DecryptUpdate(session,
                                    &crypt[i],
                                    DES_BLOCK_SIZE, &decrypt[k], &tmp);
        if (rc != CKR_OK) {
            testcase_error("C_DecryptUpdate rc=%s", p11_get_ckr(rc));
            goto error;
        }
        k += tmp;
        i += DES_BLOCK_SIZE;
    }

    decrypt_len = sizeof(decrypt) - k;

    /** finalize multipart decryption **/
    rc = funcs->C_DecryptFinal(session, &decrypt[k], &decrypt_len);
    if (rc != CKR_OK) {
        testcase_error("C_DecryptFinal rc=%s", p11_get_ckr(rc));
        return rc;
    }

    decrypt_len += k;

    /** compare encrypt/decrypt results with original data **/
    testcase_new_assertion();

    if (decrypt_len != original_len) {
        testcase_fail("decrypted multipart data length does not "
                      "match original data length.\nexpected length=%ld,"
                      " but found length=%ld\n", original_len, decrypt_len);
    } else if (memcmp(decrypt, original, original_len)) {
        testcase_fail("decrypted data does not match original " "data");
    } else {
        testcase_pass("%s Multipart Encryption/Decryption with key"
                      "generation test passed.", tsuite->name);
    }

    /** clean up **/
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_DestroyObject rc=%s", p11_get_ckr(rc));
    }

    goto testcase_cleanup;

error:
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK)
        testcase_error("C_DestroyObject rc=%s.", p11_get_ckr(rc));

testcase_cleanup:
    testcase_user_logout();
    rc = funcs->C_CloseAllSessions(slot_id);
    if (rc != CKR_OK) {
        testcase_error("C_CloseAllSessions rc=%s", p11_get_ckr(rc));
    }

    return rc;
}

/** Tests DES encryption & decryption with wrapped/unwrapped keys. **/
CK_RV do_WrapUnwrapDES(struct generated_test_suite_info * tsuite)
{
    unsigned int j;
    CK_BYTE expected[BIG_REQUEST + DES_BLOCK_SIZE];
    CK_BYTE actual[BIG_REQUEST + DES_BLOCK_SIZE];
    CK_ULONG expected_len, actual_len, cipher_len;

    CK_BYTE wrapped_data[BIG_REQUEST + DES_BLOCK_SIZE];
    CK_BYTE user_pin[PKCS11_MAX_PIN_LEN];
    CK_SESSION_HANDLE session;
    CK_MECHANISM mechkey, mech;
    CK_OBJECT_HANDLE h_key;
    CK_OBJECT_HANDLE w_key;
    CK_OBJECT_HANDLE uw_key;
    CK_ULONG wrapped_data_len;
    CK_ULONG user_pin_len;
    CK_ULONG tmpl_count = 2;
    CK_FLAGS flags;
    CK_RV rc;
    CK_OBJECT_CLASS key_class = CKO_SECRET_KEY;
    CK_KEY_TYPE key_type = CKK_DES;
    CK_SLOT_ID slot_id = SLOT_ID;

    CK_ATTRIBUTE template[] = {
        {CKA_CLASS, &key_class, sizeof(key_class)}
        ,
        {CKA_KEY_TYPE, &key_type, sizeof(key_type)}
    };

    /** begin test **/
    testcase_begin("%s Wrap/Unwrap key test.", tsuite->name);
    testcase_rw_session();
    testcase_user_login();

    /** skip test if the slot doesn't support this mechanism **/
    if (!mech_supported(slot_id, tsuite->mech.mechanism)) {
        testcase_skip("Slot %u doesn't support %s (0x%x)",
                      (unsigned int) slot_id,
                      mech_to_str(tsuite->mech.mechanism),
                      (unsigned int) tsuite->mech.mechanism);
        goto testcase_cleanup;
    }
    if (!wrap_supported(slot_id, tsuite->mech)) {
        testcase_skip("Slot %u doesn't support %s (0x%x)",
                      (unsigned int) slot_id,
                      mech_to_str(tsuite->mech.mechanism),
                      (unsigned int) tsuite->mech.mechanism);
        goto testcase_cleanup;
    }

    /** clear buffers **/
    memset(actual, 0, sizeof(actual));
    memset(expected, 0, sizeof(expected));

    /** generate data **/
    actual_len = expected_len = BIG_REQUEST;
    cipher_len = BIG_REQUEST + DES_BLOCK_SIZE;
    for (j = 0; j < actual_len; j++) {
        actual[j] = j % 255;
        expected[j] = j % 255;
    }

    /** set mechkey for key generation **/
    mechkey = des_keygen;

    /** generate a DES Key **/
    rc = funcs->C_GenerateKey(session, &mechkey, NULL, 0, &h_key);
    if (rc != CKR_OK) {
        testcase_error("C_GenerateKey rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** generate wrapping key **/
    rc = funcs->C_GenerateKey(session, &mechkey, NULL, 0, &w_key);
    if (rc != CKR_OK) {
        testcase_error("C_GenerateKey rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** set mech for crypto**/
    mech = tsuite->mech;

    /** initialize single encryption **/
    rc = funcs->C_EncryptInit(session, &mech, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_EncryptInit rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** do single encryption **/
    rc = funcs->C_Encrypt(session, actual, actual_len, actual, &cipher_len);
    if (rc != CKR_OK) {
        testcase_error("C_Encrypt rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** wrap key **/
    wrapped_data_len = sizeof(wrapped_data);
    rc = funcs->C_WrapKey(session,
                          &mech,
                          w_key,
                          h_key, (CK_BYTE *) & wrapped_data, &wrapped_data_len);
    if (rc != CKR_OK) {
        testcase_error("C_WrapKey rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** unwrap key **/
    rc = funcs->C_UnwrapKey(session, &mech, w_key, wrapped_data,
                            wrapped_data_len, template, tmpl_count, &uw_key);
    if (rc != CKR_OK) {
        testcase_error("C_UnwrapKey rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** decrypt data with unwrapped key **/
    rc = funcs->C_DecryptInit(session, &mech, uw_key);
    if (rc != CKR_OK) {
        testcase_error("C_DecryptInit rc=%s", p11_get_ckr(rc));
        goto error;
    }

    rc = funcs->C_Decrypt(session, actual, cipher_len, actual, &actual_len);
    if (rc != CKR_OK) {
        testcase_error("C_Decrypt rc=%s", p11_get_ckr(rc));
        goto error;
    }

    /** compare encrypted/decrypted data with original data **/
    testcase_new_assertion();

    if (actual_len != expected_len) {
        testcase_fail("expected length=%ld, but found length=%ld"
                      "\n", expected_len, actual_len);
        rc = CKR_GENERAL_ERROR;
    } else if (memcmp(actual, expected, actual_len)) {
        testcase_fail("decrypted data does not match plaintext " "data.");
        rc = CKR_GENERAL_ERROR;
    } else {
        testcase_pass("DES Wrap/UnWrap test for %s passed.", tsuite->name);
    }

    /** clean up **/
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK) {
        testcase_error("C_DestroyObject rc=%s", p11_get_ckr(rc));
    }
    goto testcase_cleanup;

error:
    rc = funcs->C_DestroyObject(session, h_key);
    if (rc != CKR_OK)
        testcase_error("C_DestroyObject rc=%s.", p11_get_ckr(rc));

testcase_cleanup:
    testcase_user_logout();
    rc = funcs->C_CloseAllSessions(slot_id);
    if (rc != CKR_OK) {
        testcase_error("C_CloseAllSessions rc=%s", p11_get_ckr(rc));
    }

    return rc;
}

CK_RV des_funcs()
{
    int i;
    CK_RV rv;

    /** published (known answer) tests **/
    for (i = 0; i < NUM_OF_PUBLISHED_TESTSUITES; i++) {
        rv = do_EncryptDES(&published_test_suites[i]);
        if (rv != CKR_OK && (!no_stop))
            break;

        rv = do_DecryptDES(&published_test_suites[i]);
        if (rv != CKR_OK && (!no_stop))
            break;

        rv = do_EncryptUpdateDES(&published_test_suites[i]);
        if (rv != CKR_OK && (!no_stop))
            break;

        rv = do_DecryptUpdateDES(&published_test_suites[i]);
        if (rv != CKR_OK && (!no_stop))
            break;
    }

    /** generated tests **/
    for (i = 0; i < NUM_OF_GENERATED_TESTSUITES; i++) {
        rv = do_WrapUnwrapDES(&generated_test_suites[i]);
        if (rv != CKR_OK && (!no_stop))
            break;
        rv = do_EncryptDecryptDES(&generated_test_suites[i]);
        if (rv != CKR_OK && (!no_stop))
            break;
        rv = do_EncryptDecryptUpdateDES(&generated_test_suites[i]);
        if (rv != CKR_OK && (!no_stop))
            break;
    }

    return rv;
}

int main(int argc, char **argv)
{
    int rc;
    CK_C_INITIALIZE_ARGS cinit_args;

    rc = do_ParseArgs(argc, argv);
    if (rc != 1) {
        return rc;
    }

    printf("Using slot #%lu...\n\n", SLOT_ID);
    printf("With option: no_stop: %d\n", no_stop);

    rc = do_GetFunctionList();
    if (!rc) {
        testcase_error("do_GetFunctionList(), rc=%s", p11_get_ckr(rc));
        return rc;
    }

    memset(&cinit_args, 0x0, sizeof(cinit_args));
    cinit_args.flags = CKF_OS_LOCKING_OK;

    funcs->C_Initialize(&cinit_args);
    {
        CK_SESSION_HANDLE hsess = 0;
        rc = funcs->C_GetFunctionStatus(hsess);
        if (rc != CKR_FUNCTION_NOT_PARALLEL) {
            return rc;
        }

        rc = funcs->C_CancelFunction(hsess);
        if (rc != CKR_FUNCTION_NOT_PARALLEL) {
            return rc;
        }
    }
    rc = 0;
    testcase_setup();
    rc = des_funcs();
    testcase_print_result();

    funcs->C_Finalize(NULL);

    return testcase_return(rc);
}
