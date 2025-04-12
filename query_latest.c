// query_latest.c
#include <stdio.h>
#include <sqlite3.h>

int main() {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    // Open the SQLite database
    if (sqlite3_open("energy.db", &db)) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Query: Latest entry for each appliance_id
    const char *sql =
        "SELECT timestamp, appliance_id, power_consumption, cumulative_energy, status "
        "FROM energy_data "
        "WHERE id IN ("
        "   SELECT MAX(id) FROM energy_data GROUP BY appliance_id"
        ") ORDER BY appliance_id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("Latest Logged Data:\n");
    printf("-------------------------------------------------------------------------------------\n");
    printf("| Timestamp           | Appliance | Power (W) | Energy (kWh) |     Status      |\n");
    printf("-------------------------------------------------------------------------------------\n");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *timestamp = (const char *)sqlite3_column_text(stmt, 0);
        int appliance_id = sqlite3_column_int(stmt, 1);
        double power = sqlite3_column_double(stmt, 2);
        double energy = sqlite3_column_double(stmt, 3);
        const char *status = (const char *)sqlite3_column_text(stmt, 4);

        printf("| %-19s |     %2d     | %8.2f |     %6.3f   | %-15s |\n",
               timestamp, appliance_id, power, energy, status);
    }

    printf("-------------------------------------------------------------------------------------\n");

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}
