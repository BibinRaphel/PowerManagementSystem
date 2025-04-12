-- energy_schema.sql
CREATE TABLE IF NOT EXISTS energy_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT,
    appliance_id INTEGER,
    power_consumption REAL,
    cumulative_energy REAL
);
