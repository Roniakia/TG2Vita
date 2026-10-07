// This auth port stores its session in TDLib's binlog. No SQLite file VFS is
// registered: opening a persistent SQLite database must fail, never fake locking.
#include "sqlite/sqlite3.h"
int tdsqlite3_os_init(void) { return SQLITE_OK; }
int tdsqlite3_os_end(void) { return SQLITE_OK; }
