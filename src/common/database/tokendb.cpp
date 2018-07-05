#include "tokendb.h"

#include <lmdb.h>
#include <string>
#include <boost/filesystem.hpp>
#include <cassert>
#include <iostream>

namespace Database
{

const char* const kDBFileName = "token.db";
const size_t kMapSize = 1000000000000;

TokenDB::TokenDB() : m_environment(nullptr)
{
    if (!boost::filesystem::exists(kDBFileName))
    {
        std::cout << "Database not exists, creating new one" << std::endl;
        boost::filesystem::create_directories(kDBFileName);
    }

    int rv = 0;
    rv = mdb_env_create(&m_environment);
    if (!rv && m_environment)
    {
        mdb_env_set_mapsize(m_environment, kMapSize);
        if (mdb_env_open(m_environment, kDBFileName, 0, 0644) != 0)
        {
            m_environment = nullptr;
        }
    }
}

TokenDB::~TokenDB()
{
    if (m_environment)
    {
        mdb_env_close(m_environment);
        m_environment = nullptr;
    }
}

bool TokenDB::create_token(uint32_t user_id, uint32_t token_id, uint64_t balance)
{
    if (!m_environment) return false;

    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_val key, value;

    if (mdb_txn_begin(m_environment, nullptr, 0, &txn) != 0)
    {
//        std::cerr << "Txn Begin error:"<< std::endl;
        return false;
    }

    if (mdb_open(txn, nullptr, MDB_INTEGERKEY, &dbi) !=0 )
    {
//        std::cerr << "Mdb open error:"<< std::endl;
        return false;
    }

    uint64_t key_data = 0;
    key_data = user_id;
    key_data <<= (sizeof(uint32_t) * 8);
    key_data += token_id;

    uint64_t value_data = balance;
    key.mv_size = sizeof(uint64_t);
    key.mv_data = &key_data;
    value.mv_size = sizeof(value_data);
    value.mv_data = &value_data;

    int rv = 0;
    if (!rv && (rv = mdb_put(txn, dbi, &key, &value, MDB_APPENDDUP)) != 0)
    {
//        std::cerr << "Put error code:"<<rv<<std::endl;
        mdb_txn_abort(txn);
    }

    if (!rv)
    {
        rv = mdb_txn_commit(txn);
    }

    mdb_close(m_environment, dbi);

    return !rv;
}

bool TokenDB::add_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_add)
{
    return edit_tokens(EditType::ADD, user_id, token_id, value_to_add);
}

bool TokenDB::remove_tokens(uint32_t user_id, uint32_t token_id, uint64_t value_to_remove)
{
    return edit_tokens(EditType::REMOVE, user_id, token_id, value_to_remove);
}

bool TokenDB::move_tokens(uint32_t src_user_id, uint32_t dest_user_id, uint32_t token_id, uint64_t value)
{
    if (!is_exists_token(dest_user_id, token_id)) return false;

    if (edit_tokens(EditType::REMOVE, src_user_id, token_id, value))
    {
        return edit_tokens(EditType::ADD, dest_user_id, token_id, value);
    }

    return false;
}

bool TokenDB::edit_tokens(EditType mod, uint32_t user_id, uint32_t token_id, uint64_t newvalue)
{
    if (!m_environment) return false;

    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_val key, value;
    MDB_cursor *cursor;

    if (mdb_txn_begin(m_environment, nullptr, 0, &txn) != 0)
    {
//        std::cerr << "Txn Begin error"<< std::endl;
        return false;
    }

    if (mdb_open(txn, nullptr, MDB_INTEGERKEY, &dbi) !=0 )
    {
//        std::cerr << "Mdb open error"<< std::endl;
        return false;
    }

    uint64_t key_data = user_id;
    key_data = key_data << sizeof(uint32_t) * 8;
    key_data += token_id;

    key.mv_size = sizeof(key_data);
    key.mv_data = &key_data;

    uint64_t value_data;
    value.mv_size = sizeof(value_data);
    value.mv_data = &value_data;

    int rv = 0;
    if (!rv && (rv = mdb_cursor_open(txn, dbi, &cursor)) != 0)
    {
//        std::cerr << "Cursor open error:" << rv << std::endl;
        cursor = nullptr;
    }

    if (!rv && (rv = mdb_cursor_get(cursor, &key, &value, MDB_SET)) != 0)
    {
//        std::cerr << "Cursor get error:"<< rv <<std::endl;
        cursor = nullptr;
    }

    value_data = *(uint64_t*)value.mv_data;
    if (mod == EditType::ADD)
    {
        value_data += newvalue;
    } else
    {
        if (value_data < newvalue)
        {
            rv = -1;
            cursor = nullptr;
        } else
        {
            value_data -= newvalue;
        }
    }
    value.mv_size = sizeof(uint64_t);
    value.mv_data = &value_data;

    if (!rv && (rv = mdb_cursor_put(cursor, &key, &value, MDB_CURRENT)) != 0)
    {
//        std::cerr << "Cursor put error:" << rv << std::endl;
    }

    if (rv == 0)
    {
        mdb_txn_commit(txn);
    } else
    {
//        std::cerr << "Txn abort"<<std::endl;
        mdb_txn_abort(txn);
    }

    if (mdb_cursor_txn(cursor) != nullptr)
    {
        mdb_cursor_close(cursor);
    }
    mdb_close(m_environment, dbi);
    return !rv;
}

bool TokenDB::remove_token_account(uint32_t, uint32_t )
{
    if (!m_environment) return false;


    return true;
}

bool TokenDB::is_exists_token(uint32_t user_id, uint32_t token_id)
{
    if (!m_environment) return false;

    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_val key, value;

    if (mdb_txn_begin(m_environment, nullptr, MDB_RDONLY, &txn) != 0)
    {
//        std::cerr << "Txn Begin error:"<< std::endl;
        return false;
    }

    if (mdb_open(txn, nullptr, MDB_INTEGERKEY, &dbi) !=0 )
    {
//        std::cerr << "Mdb open error:"<< std::endl;
        return false;
    }

    uint64_t key_data = 0;
    key_data = user_id;
    key_data <<= (sizeof(uint32_t) * 8);
    key_data += token_id;

    uint64_t value_data;
    key.mv_size = sizeof(uint64_t);
    key.mv_data = &key_data;
    value.mv_size = sizeof(value_data);
    value.mv_data = &value_data;

    uint64_t rv = 0;
    if (!rv && (rv = mdb_get(txn, dbi, &key, &value)) != 0)
    {
//        std::cerr << "Get error code:"<<rv<<std::endl;
    }

    mdb_txn_abort(txn);
    mdb_close(m_environment, dbi);

    return !rv;
}

uint64_t TokenDB::get_balance(uint32_t user_id, uint32_t token_id)
{
    if (!m_environment) return false;

    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_val key, value;

    if (mdb_txn_begin(m_environment, nullptr, MDB_RDONLY, &txn) != 0)
    {
//        std::cerr << "Txn Begin error:"<< std::endl;
        return false;
    }

    if (mdb_open(txn, nullptr, MDB_INTEGERKEY, &dbi) !=0 )
    {
//        std::cerr << "Mdb open error:"<< std::endl;
        return false;
    }

    uint64_t key_data = 0;
    key_data = user_id;
    key_data <<= (sizeof(uint32_t) * 8);
    key_data += token_id;

    uint64_t value_data;
    key.mv_size = sizeof(uint64_t);
    key.mv_data = &key_data;
    value.mv_size = sizeof(value_data);
    value.mv_data = &value_data;

    uint64_t rv = 0;
    if (!rv && (rv = mdb_get(txn, dbi, &key, &value)) != 0)
    {
//        std::cerr << "Get error code:"<<rv<<std::endl;
    } else
    {
        rv = *(uint64_t*)value.mv_data;
    }

    mdb_txn_abort(txn);
    mdb_close(m_environment, dbi);

    return rv;
}

#ifdef DEBUG

#include <stdio.h>
void TokenDB::dump()
{
    if (!m_environment) return;
    std::cout<< "****** DB DUMP ******" << std::endl;

    MDB_txn *txn;
    MDB_dbi dbi;
    MDB_val key, value;
    MDB_cursor *cursor;

    mdb_txn_begin(m_environment, nullptr, 0, &txn);
    mdb_open(txn, nullptr, MDB_INTEGERKEY, &dbi);

    uint64_t key_data;
    key.mv_size = sizeof(key_data);
    key.mv_data = &key_data;

    uint64_t value_data;
    value.mv_size = sizeof(value_data);
    value.mv_data = &value_data;


    mdb_cursor_open(txn, dbi, &cursor);
    while (mdb_cursor_get(cursor, &key, &value, MDB_NEXT) == 0)
    {
        uint64_t fullkey = *(uint64_t*)key.mv_data;
        std::cout << (fullkey>>32) <<" "<<(uint32_t)fullkey<<" "<<*(uint64_t*)value.mv_data<<std::endl;
    }

    mdb_cursor_put(cursor, &key, &value, MDB_CURRENT);
    mdb_txn_abort(txn);
    mdb_cursor_close(cursor);
    mdb_close(m_environment, dbi);
    std::cout<< "****** DB DUMP END ***" << std::endl;
}
#endif /*DEBUG*/

}
