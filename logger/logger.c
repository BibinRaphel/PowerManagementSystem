// logger.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string.h>
#include <curl/curl.h>
#include <stdbool.h>

#define TOTAL_DEVICES           3
#define MAX_ENTRIES             15        // Max entries to retain in DB
#define MAX_RETRIES             3         // Max retries for sending data
#define SENSOR_MIN              10        // Simulated min valid sensor power value
#define SENSOR_MAX              400       // Simulated max valid sensor power value
#define NORMAL_OPERATION        0X00      // Normal Operation
#define OVER_CONSUMPTION        0X01      // Over consumption of power
#define DEVICE_DISCONNECTED     0X02      // Device Disconnected
const char* get_timestamp()
{
    static char buffer[20];
    time_t now = time(NULL);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buffer;
}

int is_sensor_connected(float power)
{
    int powerStaus = 0;

    if(power > SENSOR_MAX)
    {
        powerStaus = OVER_CONSUMPTION;
    }
    else if(power <= SENSOR_MIN)
    {
        powerStaus = DEVICE_DISCONNECTED;
    }
    else
    {
        powerStaus = NORMAL_OPERATION;
    }

    return powerStaus;
}

bool insert_device_data(sqlite3 *db, int appliance_id, float power, float energy, const char *status)
{
    const char *sql =
        "INSERT INTO energy_data (timestamp, appliance_id, power_consumption, cumulative_energy, status) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "SQL error (prepare): %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, get_timestamp(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, appliance_id);
    sqlite3_bind_double(stmt, 3, power);
    sqlite3_bind_double(stmt, 4, energy);
    sqlite3_bind_text(stmt, 5, status, -1, SQLITE_STATIC);

    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    if (!success)
    {
        fprintf(stderr, "Insert failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return success;
}


void insert_data(sqlite3 *db)
{
    for (int idx = 0; idx < TOTAL_DEVICES; idx++)
    {
        float power = (float)(rand() % 450);  // Simulated range, includes possible disconnection
        float energy = power * 0.001f;

        if (is_sensor_connected(power) == OVER_CONSUMPTION)
        {
            fprintf(stderr, "Sensor %d disconnected: Power = %.2f W\n", idx, power);

            if (insert_device_data(db, idx, -1.0f, -1.0f, "Over Consumption Device Turned off"))
            {
                printf("Logged sensor %d disconnection in database.\n", idx);
            }
            continue;
        }
        else if (is_sensor_connected(power) == DEVICE_DISCONNECTED)
        {
            fprintf(stderr, "Sensor %d disconnected: Power = %.2f W\n", idx, power);

            if (insert_device_data(db, idx, -1.0f, -1.0f, "Device Not connected"))
            {
                printf("Logged sensor %d disconnection in database.\n", idx);
            }
            continue;
        }

        if (insert_device_data(db, idx, power, energy, "OK"))
        {
            printf("Time: %s | Appliance: %d | Power: %.2f W | Energy: %.3f kWh | Status: OK\n",
                   get_timestamp(), idx, power, energy);
        }
    }
}



bool send_data_to_server(const char *json)
{
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/upload");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);
    if (!success)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return success;
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
                 "{\"timestamp\":\"%s\",\"appliance_id\":%d,\"power_consumption\":%.2f,\"cumulative_energy\":%.3f,\"status\":\"%s\"}",
                 sqlite3_column_text(stmt, 1),
                 sqlite3_column_int(stmt, 2),
                 sqlite3_column_double(stmt, 3),
                 sqlite3_column_double(stmt, 4),
                 sqlite3_column_text(stmt, 5));
        strcat(json, entry);
    }
    strcat(json, "]");
    sqlite3_finalize(stmt);

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt)
    {
        if (send_data_to_server(json))
        {
            printf("Data sent successfully (Attempt %d).\n", attempt);
            return;
        }
        else
        {
            fprintf(stderr, "Retrying send (Attempt %d)...\n", attempt);
            sleep(2);
        }
    }

    fprintf(stderr, "Failed to send data after %d attempts.\n", MAX_RETRIES);
}

void keep_latest_entries(sqlite3 *db)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM energy_data WHERE id NOT IN (SELECT id FROM energy_data ORDER BY id DESC LIMIT %d);",
             MAX_ENTRIES);

    char *errmsg = NULL;
    if (sqlite3_exec(db, sql, 0, 0, &errmsg) != SQLITE_OK)
    {
        fprintf(stderr, "Cleanup error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}

int main()
{
    sqlite3 *db;
    srand(time(NULL));

    if (sqlite3_open(".build/energy.db", &db))
    {
        fprintf(stderr, "Database open error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("Energy logger started. Ctrl+C to stop.\n");

    while (true)
    {
        insert_data(db);          // Inser to Database
        fetch_and_send(db);       // Update Web Server
        keep_latest_entries(db);  // Clean up old rows
        sleep(5);                 // log every 5
    }

    sqlite3_close(db);
    return 0;
}
