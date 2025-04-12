# Paths
BUILD_DIR = .build
DB_FILE = $(BUILD_DIR)/energy.db
SQL_FILE = sql/energy_schema.sql
LOGGER_SRC = logger/logger.c
QUERY_SRC = user_query/query_latest.c
LOGGER_EXE = $(BUILD_DIR)/logger
QUERY_EXE = $(BUILD_DIR)/query_latest
SERVER_SCRIPT = server/server.py

# Default target
all: setup build

# Create .build directory and initialize DB
setup:
	mkdir -p $(BUILD_DIR)
	sqlite3 $(DB_FILE) < $(SQL_FILE)

# Build all executables
build: $(LOGGER_EXE) $(QUERY_EXE)

$(LOGGER_EXE): $(LOGGER_SRC)
	gcc $< -lsqlite3 -o $@ -lcurl

$(QUERY_EXE): $(QUERY_SRC)
	gcc $< -o $@ -lsqlite3

# Run Python server (runs in background)
run_server:
	@echo "Starting server..."
	python3 $(SERVER_SCRIPT) &

# Run logger (ensures server is running first)
run_logger: run_server
	@echo "Running logger..."
	$(LOGGER_EXE)

# Run query_latest
query:
	$(QUERY_EXE)

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
