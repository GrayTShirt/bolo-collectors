--- standard PostgreSQL health queries
--- per http://www.postgresql.org/docs/9.4/static/monitoring-stats.html#MONITORING-STATS-VIEWS
---

SELECT 'bg.checkspoints_timed'    AS "name", 'RATE' AS "type", checkpoints_timed     AS "value" FROM pg_stat_bgwriter;
SELECT 'bg.checkpoints_req'       AS "name", 'RATE' AS "type", checkpoints_req       AS "value" FROM pg_stat_bgwriter;
SELECT 'bg.buffers_checkpoint'    AS "name", 'RATE' AS "type", buffers_checkpoint    AS "value" FROM pg_stat_bgwriter;
SELECT 'bg.buffers_clean'         AS "name", 'RATE' AS "type", buffers_clean         AS "value" FROM pg_stat_bgwriter;
SELECT 'bg.maxwritten_clean'      AS "name", 'RATE' AS "type", maxwritten_clean      AS "value" FROM pg_stat_bgwriter;
SELECT 'bg.buffers_backend'       AS "name", 'RATE' AS "type", buffers_backend       AS "value" FROM pg_stat_bgwriter;
SELECT 'bg.buffers_backend_fsync' AS "name", 'RATE' AS "type", buffers_backend_fsync AS "value" FROM pg_stat_bgwriter;
SELECT 'bg.buffers_alloc'         AS "name", 'RATE' AS "type", buffers_alloc         AS "value" FROM pg_stat_bgwriter;

SELECT 'db:' || datname || ':conn.waiting'   AS "name", 'SAMPLE' AS "type", SUM(CASE WHEN waiting='t' THEN 1 ELSE 0 END) AS "value" FROM pg_stat_activity GROUP BY datname;
SELECT 'db:' || datname || ':conn.active'    AS "name", 'SAMPLE' AS "type", SUM(CASE WHEN waiting='t' THEN 0 ELSE 1 END) AS "value" FROM pg_stat_activity GROUP BY datname;

SELECT 'db:' || datname || ':locks.all'      AS "name", 'SAMPLE' AS "type", COUNT(*) AS "value" FROM pg_locks INNER JOIN pg_database ON database = oid GROUP BY datname;
-- FIXME: may want to break out by mode, and provide more visibility into potential locking issues

SELECT 'db:' || datname || ':size.bytes'     AS "name", 'SAMPLE' AS "type", pg_database_size(datname) FROM pg_stat_database;
SELECT 'db:' || datname || ':backends'       AS "name", 'SAMPLE' AS "type", numbackends    AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':xact_commit'    AS "name", 'RATE'   AS "type", xact_commit    AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':xact_rollback'  AS "name", 'RATE'   AS "type", xact_rollback  AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':blks_read'      AS "name", 'RATE'   AS "type", blks_read      AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':blks_hit'       AS "name", 'RATE'   AS "type", blks_hit       AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':tup_returned'   AS "name", 'RATE'   AS "type", tup_returned   AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':tup_fetched'    AS "name", 'RATE'   AS "type", tup_fetched    AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':tup_inserted'   AS "name", 'RATE'   AS "type", tup_inserted   AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':tup_updated'    AS "name", 'RATE'   AS "type", tup_updated    AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':tup_deleted'    AS "name", 'RATE'   AS "type", tup_deleted    AS "value" FROM pg_stat_database;
SELECT 'db:' || datname || ':conflicts'      AS "name", 'RATE'   AS "type", conflicts      AS "value" FROM pg_stat_database;
--SELECT 'db:' || datname || ':temp_files'     AS "name", 'RATE'   AS "type", temp_files     AS "value" FROM pg_stat_database;
--SELECT 'db:' || datname || ':temp_bytes'     AS "name", 'RATE'   AS "type", temp_bytes     AS "value" FROM pg_stat_database;
--SELECT 'db:' || datname || ':deadlocks'      AS "name", 'RATE'   AS "type", deadlocks      AS "value" FROM pg_stat_database;
--SELECT 'db:' || datname || ':blk_read_time'  AS "name", 'RATE'   AS "type", blk_read_time  AS "value" FROM pg_stat_database;
--SELECT 'db:' || datname || ':blk_write_time' AS "name", 'RATE'   AS "type", blk_write_time AS "value" FROM pg_stat_database;

SELECT 'db:' || datname || ':conflict.tablespace' AS "name", 'RATE' AS "type", confl_tablespace AS "value" FROM pg_stat_database_conflicts;
SELECT 'db:' || datname || ':conflict.lock'       AS "name", 'RATE' AS "type", confl_lock       AS "value" FROM pg_stat_database_conflicts;
SELECT 'db:' || datname || ':conflict.snapshot'   AS "name", 'RATE' AS "type", confl_snapshot   AS "value" FROM pg_stat_database_conflicts;
SELECT 'db:' || datname || ':conflict.bufferpin'  AS "name", 'RATE' AS "type", confl_bufferpin  AS "value" FROM pg_stat_database_conflicts;
SELECT 'db:' || datname || ':conflict.deadlock'   AS "name", 'RATE' AS "type", confl_deadlock   AS "value" FROM pg_stat_database_conflicts;
