#ifndef REACH_PORTS_USER_ACCOUNT_H
#define REACH_PORTS_USER_ACCOUNT_H

#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_USER_ACCOUNT_NAME_CAPACITY 128

    typedef struct reach_user_account reach_user_account;

    typedef struct reach_user_account_info
    {
        uint16_t display_name[REACH_USER_ACCOUNT_NAME_CAPACITY];
        uint16_t user_name[REACH_USER_ACCOUNT_NAME_CAPACITY];
        int32_t is_administrator;
        uint64_t picture_icon_id;
    } reach_user_account_info;

    typedef enum reach_user_account_password_status
    {
        REACH_USER_ACCOUNT_PASSWORD_CHANGED = 0,
        REACH_USER_ACCOUNT_PASSWORD_WRONG_CURRENT = 1,
        REACH_USER_ACCOUNT_PASSWORD_POLICY = 2,
        REACH_USER_ACCOUNT_PASSWORD_FAILED = 3
    } reach_user_account_password_status;

    typedef struct reach_user_account_ops
    {
        reach_result (*query)(reach_user_account *account, reach_user_account_info *out_info);
        reach_result (*verify_password)(reach_user_account *account, const uint16_t *password,
                                        int32_t *out_valid);
        reach_result (*change_password)(reach_user_account *account,
                                        const uint16_t *current_password,
                                        const uint16_t *new_password,
                                        reach_user_account_password_status *out_status);
        void (*destroy)(reach_user_account *account);
    } reach_user_account_ops;

    typedef struct reach_user_account_port
    {
        reach_user_account *account;
        reach_user_account_ops ops;
    } reach_user_account_port;

#ifdef __cplusplus
}
#endif

#endif
