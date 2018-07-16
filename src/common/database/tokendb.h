#ifndef TOKENDB_H
#define TOKENDB_H

#include <stdint.h>

class MDB_env;

namespace Database
{

class TokenDB
{
public:
    TokenDB();
    ~TokenDB();

    bool create_token(uint32_t user_id, uint32_t token_id, uint64_t balance);
    bool add_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_add);
    bool remove_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_remove);
    bool move_tokens(uint32_t src_user_id, uint32_t dest_user_id, uint32_t token_id, uint64_t value);
    bool remove_token_account(uint32_t user_id, uint32_t token_id);

    bool is_exists_token(uint32_t user_id, uint32_t token_id);
    uint64_t get_balance(uint32_t user_id, uint32_t token_id);
    uint32_t get_tokens_count();
#ifdef DEBUG
    void dump();
#endif /*DEBUG*/

private:
    enum EditType {ADD, REMOVE};

    bool edit_tokens(EditType mod, uint32_t user_id, uint32_t token_id, uint64_t newvalue);

    ::MDB_env *m_environment;
};

}
#endif // TOKENDB_H
