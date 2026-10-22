/*
 * Copyright (C) 2018-2019 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#include "lexbor/core/shs.h"
#include "lexbor/core/str.h"

#define LEXBOR_STR_RES_MAP_LOWERCASE
#define LEXBOR_STR_RES_MAP_UPPERCASE
#include "lexbor/core/str_res.h"


#define lexbor_shs_make_id_m(key, size, table_size)                            \
    (((((key[0] * key[size - 1]) * key[0]) + size) % table_size) + 0x01)

#define lexbor_shs_make_id_lower_m(key, size, table_size)                      \
    (((((lexbor_str_res_map_lowercase[key[0]]                                  \
     * lexbor_str_res_map_lowercase[key[size - 1]])                            \
     * lexbor_str_res_map_lowercase[key[0]])                                   \
     + size)                                                                   \
     % table_size) + 0x01)

#define lexbor_shs_make_id_upper_m(key, size, table_size)                      \
    (((((lexbor_str_res_map_uppercase[key[0]]                                  \
     * lexbor_str_res_map_uppercase[key[size - 1]])                            \
     * lexbor_str_res_map_uppercase[key[0]])                                   \
     + size)                                                                   \
     % table_size) + 0x01)


const lexbor_shs_entry_t *
lexbor_shs_entry_get_static(const lexbor_shs_entry_t *root,
                            const lxb_char_t *key, size_t key_len)
{
    const lexbor_shs_entry_t *entry;
    entry = root + lexbor_shs_make_id_m(key, key_len, root->key_len);

    while (entry->key != NULL)
    {
        if (entry->key_len == key_len) {
            if (lexbor_str_data_ncmp((const lxb_char_t *) entry->key,
                                     key, key_len))
            {
                return entry;
            }

            entry = &root[entry->next];
        }
        else if (entry->key_len > key_len) {
            return NULL;
        }
        else {
            entry = &root[entry->next];
        }
    }

    return NULL;
}

const lexbor_shs_entry_t *
lexbor_shs_entry_get_lower_static(const lexbor_shs_entry_t *root,
                                  const lxb_char_t *key, size_t key_len)
{
    size_t i;
    uint32_t id;
    const lxb_char_t *ekey;
    const lexbor_shs_entry_t *entry;

    /*
     * Same id as lexbor_shs_make_id_lower_m(), with a 32-bit division when
     * the operands fit (the product of three bytes is below 2^24).
     */
    if (key_len <= UINT32_MAX - 0x01000000 && root->key_len <= UINT32_MAX) {
        id = (uint32_t) lexbor_str_res_map_lowercase[key[0]]
             * lexbor_str_res_map_lowercase[key[key_len - 1]]
             * lexbor_str_res_map_lowercase[key[0]]
             + (uint32_t) key_len;

        entry = root + (id % (uint32_t) root->key_len) + 0x01;
    }
    else {
        entry = root + lexbor_shs_make_id_lower_m(key, key_len, root->key_len);
    }

    while (entry->key != NULL)
    {
        if (entry->key_len == key_len) {
            ekey = (const lxb_char_t *) entry->key;

            for (i = 0; i < key_len; i++) {
                if (ekey[i] != lexbor_str_res_map_lowercase[key[i]]) {
                    break;
                }
            }

            if (i == key_len) {
                return entry;
            }

            entry = &root[entry->next];
        }
        else if (entry->key_len > key_len) {
            return NULL;
        }
        else {
            entry = &root[entry->next];
        }
    }

    return NULL;
}

const lexbor_shs_entry_t *
lexbor_shs_entry_get_upper_static(const lexbor_shs_entry_t *root,
                                  const lxb_char_t *key, size_t key_len)
{
    const lexbor_shs_entry_t *entry;
    entry = root + lexbor_shs_make_id_upper_m(key, key_len, root->key_len);

    while (entry->key != NULL)
    {
        if (entry->key_len == key_len) {
            if (lexbor_str_data_nupcmp_right((const lxb_char_t *) entry->key,
                                             key, key_len))
            {
                return entry;
            }

            entry = &root[entry->next];
        }
        else if (entry->key_len > key_len) {
            return NULL;
        }
        else {
            entry = &root[entry->next];
        }
    }

    return NULL;
}
