// logger.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sqlite3.h>

const char* get_timestamp()
{
    static char buffer[20];
    time_t now = time(NULL);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buffer;
}

void insert_data(sqlite3 *db)
{
    const char *sql = "INSERT INTO energy_data (timestamp, appliance_id, power_consumption, cumulative_energy) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return;
    }

    int appliance_id = (rand() % 4) + 1;  // 1 to 4
    float power = (float)(rand() % 300 + 100);   // 100W to 400W
    float energy = power * 0.001f;               // Simulated cumulative energy

    sqlite3_bind_text(stmt, 1, get_timestamp(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, appliance_id);
    sqlite3_bind_double(stmt, 3, power);
    sqlite3_bind_double(stmt, 4, energy);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        fprintf(stderr, "Insert failed: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        printf("Logged → Time: %s | Appliance ID : %d | Power: %.2f W | Energy: %.3f kWh\n",
               get_timestamp(), appliance_id, power, energy);
    }

    sqlite3_finalize(stmt);
}

int main()
{
    sqlite3 *db;
    srand(time(NULL));  // seed for random generator

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

    while (1)
    {
        insert_data(db);
        sleep(5);  // log every 5 seconds
    }

    sqlite3_close(db);
    return 0;
}
