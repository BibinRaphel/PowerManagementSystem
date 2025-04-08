// client_upload.c
// Sends SQLite data to local server via HTTP POST (JSON format)

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

void send_data_to_server(const char *json) {
    CURL *curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/upload");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
}

void fetch_and_send(sqlite3 *db) {
    const char *sql = "SELECT * FROM energy_data ORDER BY id DESC LIMIT 5";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return;
    }

    char json[2048] = "[";
    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
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

int main() {
    sqlite3 *db;
    if (sqlite3_open("energy.db", &db)) {
        fprintf(stderr, "Database open error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

	while(1)
	{
		fetch_and_send(db);
		sleep(5);  // log every 5 seconds
	}

    sqlite3_close(db);
    return 0;
}
