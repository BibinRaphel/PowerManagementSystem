// logger.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <string.h>
#include <curl/curl.h>

#define TOTAL_DEVICES 3
#define MAX_ENTRIES   15  // Change this to keep more or fewer entries in DB

const char* get_timestamp()
{
    static char buffer[20];
    time_t now = time(NULL);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buffer;
}

void insert_data(sqlite3 *db)
{
    for (int idx = 0; idx < TOTAL_DEVICES; idx++)
    {
        const char *sql = "INSERT INTO energy_data (timestamp, appliance_id, power_consumption, cumulative_energy) VALUES (?, ?, ?, ?);";
        sqlite3_stmt *stmt;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
            return;
        }

        float power = (float)(rand() % 300 + 100);  // 100W to 400W
        float energy = power * 0.001f;              // Simulated cumulative energy

        sqlite3_bind_text(stmt, 1, get_timestamp(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, idx);
        sqlite3_bind_double(stmt, 3, power);
        sqlite3_bind_double(stmt, 4, energy);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            fprintf(stderr, "Insert failed: %s\n", sqlite3_errmsg(db));
        }
        else
        {
            printf("Time: %s | Appliance ID: %d | Power: %.2f W | Energy: %.3f kWh\n",
                   get_timestamp(), idx, power, energy);
        }

        sqlite3_finalize(stmt);
    }
}

void send_data_to_server(const char *json)
{
    CURL *curl = curl_easy_init();
    if (curl)
    {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/upload");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void fetch_and_send(sqlite3 *db)
{
    const char *sql = "SELECT * FROM energy_data ORDER BY id DESC LIMIT 5";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return;
    }

    char json[2048] = "[";
    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (!first) strcat(json, ",");
        first = 0;

        char entry[512];
        snprintf(entry, sizeof(entry),
                 "{\"timestamp\":\"%s\",\"appliance_id\":%d,\"power_consumption\":%.2f,\"cumulative_energy\":%.3f}",
                 sqlite3_column_text(stmt, 1),
                 sqlite3_column_int(stmt, 2),
                 sqlite3_column_double(stmt, 3),
                 sqlite3_column_double(stmt, 4));
        strcat(json, entry);
    }
    strcat(json, "]");
    sqlite3_finalize(stmt);

    send_data_to_server(json);
}

void keep_latest_entries(sqlite3 *db)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM energy_data "
             "WHERE id NOT IN (SELECT id FROM energy_data ORDER BY id DESC LIMIT %d);",
             MAX_ENTRIES);

    char *errmsg = NULL;
    if (sqlite3_exec(db, sql, 0, 0, &errmsg) != SQLITE_OK)
    {
        fprintf(stderr, "Cleanup error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}

int main() {
    sqlite3 *db;
    srand(time(NULL));  // Seed for random generator

    if (sqlite3_open("energy.db", &db))
    {
        fprintf(stderr, "Database open error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    const char *sql_create =
        "CREATE TABLE IF NOT EXISTS energy_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp TEXT,"
        "appliance_id INTEGER,"
        "power_consumption REAL,"
        "cumulative_energy REAL);";

    char *errmsg = 0;
    if (sqlite3_exec(db, sql_create, 0, 0, &errmsg) != SQLITE_OK)
    {
        fprintf(stderr, "Table creation error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    printf("Starting data logger... Press Ctrl+C to stop.\n");

    while (true)
    {
        insert_data(db);          // Inser to Database
        fetch_and_send(db);       // Update Web Server
        keep_latest_entries(db);  // Clean up old rows
        sleep(5);                 // log every 5 seconds
    }

    sqlite3_close(db);
    return 0;
}
