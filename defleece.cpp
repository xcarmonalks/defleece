#include <stdio.h>
#include <chrono>
#include <string>
#include <cstdarg>
#include <iostream>
#include <sstream>
#include <sqlite3.h>
#include "fleece/Fleece.hh"
#include "fleece/Expert.hh"

using namespace fleece;
using namespace std;
using namespace std::chrono;

inline std::string format(const char* fmt, ...){
    int size = 512;
    char* buffer = 0;
    buffer = new char[size];
    va_list vl;
    va_start(vl, fmt);
    int nsize = vsnprintf(buffer, size, fmt, vl);
    if(size<=nsize){ //fail delete buffer and try again
        delete[] buffer;
        buffer = 0;
        buffer = new char[nsize+1]; //+1 for /0
        nsize = vsnprintf(buffer, size, fmt, vl);
    }
    std::string ret(buffer);
    va_end(vl);
    delete[] buffer;
    return ret;
}

bool fileExists(const char* path) {
    if (FILE *file = fopen(path, "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    } 
}

static void writeOutput(slice output, bool asHex =false) {
    if (asHex) {
        string hex = output.hexString();
        fputs(hex.c_str(), stdout);
    } else {
        fwrite(output.buf, 1, output.size, stdout);
    }
}

SharedKeys findSharedKeys(sqlite3 *db) {
    sqlite3_stmt* stmt;
    char q[999];
    int bytes;
    const unsigned char* blob;

    string query = "SELECT body from kv_info where key = 'SharedKeys'";
    //printf("Query: %s\n", query.c_str());

    if(sqlite3_prepare_v2(db,  query.c_str(), -1, &stmt, NULL)!= SQLITE_OK){
        printf("ERROR: while compiling sql: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        sqlite3_finalize(stmt);
        return NULL;
    }

    if(sqlite3_step (stmt) == SQLITE_ROW) {
        int size = sqlite3_column_bytes(stmt, 0);
        uint8_t* p = (uint8_t*)sqlite3_column_blob(stmt,0);

        stringstream out;
        out.write((char*)p, size);
        alloc_slice data = alloc_slice(out.str());

        SharedKeys sk = SharedKeys::create();
        Doc doc(data, kFLUntrusted, sk);
        slice output = doc.data();
        auto json = doc.root().toJSONString();
        //printf ("SharedKeys: %s ", json.c_str());
        return SharedKeys::create(output);
    }

    return NULL;

}

void prepareOutDb(sqlite3 *dbOut) {
    char *error_report = NULL;
    int result = -1;

    string drop = "DROP TABLE IF EXISTS kv_default;";
    if(result = sqlite3_exec(dbOut, drop.c_str(), 0, 0, &error_report)) {
        printf( "\t> CMD: %s , Error: %s\n" , drop.c_str() , error_report );
        sqlite3_free(error_report);
    }

    string create = "CREATE TABLE IF NOT EXISTS kv_default (key TEXT PRIMARY KEY, sequence INTEGER, flags INTEGER, body BLOB);";
    if(result = sqlite3_exec(dbOut, create.c_str(), 0, 0, &error_report)) {
        printf( "\t> CMD: %s , Error: %s\n\n" , create.c_str() , error_report );
        sqlite3_free(error_report);
    }
}

void insertIntoOutDb(sqlite3 *dbOut, string key, int sequence, int flags, string body) {
    char *error_report = NULL;
    int result = -1;
    sqlite3_stmt* stmt;

    string insert = "INSERT INTO kv_default ('key', 'sequence', 'flags', 'body') VALUES (?,?,?,?);";
    if(sqlite3_prepare_v2(dbOut,  insert.c_str(), -1, &stmt, NULL) == SQLITE_OK){
        sqlite3_bind_text(stmt, 1, key.c_str(), key.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, sequence);
        sqlite3_bind_int(stmt, 3, flags);
        sqlite3_bind_blob(stmt, 4, body.c_str(), body.size(), SQLITE_TRANSIENT);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
}

int extractDatabase(const char* path){
    sqlite3 *db;
    sqlite3 *dbOut;
    sqlite3_stmt* stmt;
    sqlite3_stmt* stmtOut;
    char q[999];
    int row = 0;
    int bytes;

    if(!fileExists(path)) {
        printf("Cannot open file %s\n", path);
        return -1;
    }

    if (sqlite3_open(path, &db) == SQLITE_OK && sqlite3_open("out.db", &dbOut) == SQLITE_OK) {
        printf("DB Opened successfully\n");
        
        string query = "SELECT key, sequence, flags, body from kv_default";

        //printf("Query: %s\n", query.c_str());

        if(sqlite3_prepare_v2(db,  query.c_str(), -1, &stmt, NULL)!= SQLITE_OK){
            printf("ERROR: while compiling sql: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            sqlite3_finalize(stmt);
            return -1;
        }

        SharedKeys sk = findSharedKeys(db);
        prepareOutDb(dbOut);

        bool done = false;
        printf("Performing extraction... \n");
        auto start = high_resolution_clock::now();
        
        while (!done) {
            switch (sqlite3_step (stmt)) {
                case SQLITE_ROW: {
                    const unsigned char* key;
                    int sequence;
                    int flags;

                    int keySize = sqlite3_column_bytes(stmt, 0);
                    key = sqlite3_column_text(stmt, 0);
                    sequence = sqlite3_column_int(stmt, 1);
                    flags = sqlite3_column_int(stmt, 2);
                    int size = sqlite3_column_bytes(stmt, 3);
                    uint8_t* p = (uint8_t*)sqlite3_column_blob(stmt,3);

                    stringstream keyStr;
                    keyStr.write((char*) key, keySize);

                    stringstream out;
                    out.write((char*)p, size);
                    alloc_slice data = alloc_slice(out.str());

                    Doc doc(data, kFLUntrusted, sk);
                    if(!doc) {
                        fprintf(stderr, "Failed to convert to doc.\n");
                        return 1;
                    }

                    slice output = doc.data();
                    
                    auto json = doc.root().toJSONString();
                    //printf ("Row: %d, Key: %s, Fleece: %s \n", row, text, json.c_str());
                    insertIntoOutDb(dbOut, keyStr.str(), sequence, flags, json);
                    row++;
                    break;
                }

                case SQLITE_DONE: {
                    done = true;
                    break;
                }

                default: {
                    fprintf(stderr, "Failed.\n");
                    return 1;
                }
            }
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        sqlite3_close(dbOut);

        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<seconds>(stop - start);
        printf("Decodified %d rows from kv_default to out.db in %ld seconds\n", row, duration.count());

        return 0;
    }

    return -1;
}


int main(int argc, char* argv[]) {

    if(argc != 2) {
        printf("Usage: fleeceExtractor <path to sqlite file>\n");
        return -1;
    } else {
        printf("Opening file %s\n", argv[1]);
        return extractDatabase(argv[1]);
    }
}