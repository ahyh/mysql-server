# ==== Purpose ====
#
# This test case will test some basic aspects of binlog_encryption option and
# how to fix possible issues.
#
# Part 1: Cannot enable the option without keyring.
#
# The server shall abort to start when it is unable to initialize the binary
# log encryption.
#
# Part 2: First time enable.
#
# The source enables the option at startup.
#
# The replica enables the option in a client session after installing the
# keyring.
#
# Verify binary log rotation, master key ID and encryption of new binary log
# files on source and replica servers.
#
# Verify that 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' rotates
# the maskter key, which is used to encrypt new created relay
# logs and re-encrypt previous encrypted relay logs.
#
# Part 3: Start the replica to replicate from the source.
#
# Ensure that the replica can replicate content from the source.
#
# Part 4: Uninstall keyring on replica before stopping replication threads.
#
# Makes the replica to fail to read existing relay log files by uninstalling
# the keyring plugin.
#
# The replica shall be able to work again after a CHANGE REPLICATION SOURCE that discards
# the existing relay log files.
#
# Part 5: Verify that 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' rotates
# the master key, which is used to encrypt new created binary
# logs and re-encrypt previous encrypted binary logs.
#
# Verify that we can not rotate binlog master key when
# 'binlog-encryption' is off.
#
# ==== Related Bugs and Worklogs ====
#
# WL#10957: Binary log encryption at rest
# BUG#28628991: ASSERTION `!IS_SET()' FAILED.
# BUG#28616149: WRONG ERR MESSAGE IF SLAVE IS NOT ABLE TO READ RELAYLOG AS KEYRING UNINSTALLED
#

# Suppression of error messages
--disable_query_log
call mtr.add_suppression("Could not find the data corresponding to Data ID:");
CALL mtr.add_suppression('Failed to store key');
CALL mtr.add_suppression('Failed to initialize binlog encryption');
CALL mtr.add_suppression('Unable to recover binlog encryption master key');
CALL mtr.add_suppression('Aborting');
CALL mtr.add_suppression('Cannot get file password for encrypted replication log file');
CALL mtr.add_suppression('Could not parse relay log event entry');
CALL mtr.add_suppression('Failed to fetch key from keyring');
CALL mtr.add_suppression('Can\'t find key from keyring');
--enable_query_log

# Setup test case variables
--let $MASTER_DATADIR= `select @@datadir`
--source include/rpl/connection_replica.inc
--let $SLAVE_DATADIR= `select @@datadir`
--source include/rpl/connection_source.inc


--echo # Part 1
--let $before_master_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)

# Enabling the option without plug-in
--error ER_RPL_ENCRYPTION_MASTER_KEY_RECOVERY_FAILED
SET @@GLOBAL.binlog_encryption = ON;

--let $assert_text=binlog_encryption option shall be OFF
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "OFF"
--source include/assert.inc

--let $after_master_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $assert_text=Binary log was not rotated
--let $assert_cond= "$before_master_file" = "$after_master_file"
--source include/assert.inc
--let $rpl_log_file=$MASTER_DATADIR$after_master_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=Binary log is not encrypted
--let $assert_cond= "$rpl_encryption_key_id" = "None"
--source include/assert.inc
--let $assert_text= 1st binary log is not encrypted on source
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 1]" = "No"
--source include/assert.inc

# Stop the source server
--let $rpl_server_number= 1
--source include/rpl/stop_server.inc
# The source will not be able to process some MTR include requests
--connection slave

# Backup source's local manifest file to start it without keyring
--let CURRENT_DATADIR = $MASTER_DATADIR
--source include/keyring_tests/helper/instance_backup_manifest.inc

# Try to restart the source server setting the option, but it should fail
--echo Starting the server with "--binlog_encryption=ON" but no keyring installed
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON --log-error=$MYSQL_TMP_DIR/master.err
--error 0,1
--exec $MYSQLD --defaults-file=$MYSQLTEST_VARDIR/my.cnf --defaults-group-suffix=.$rpl_server_number $rpl_server_parameters
--let $assert_file= $MYSQL_TMP_DIR/master.err
--let $assert_text= Server failed to initialize binlog encryption
--let $assert_select= Failed to initialize binlog encryption
--let $assert_count= 1
--source include/assert_grep.inc
--let $assert_text= Server aborted to start
--let $assert_select= Server.*Aborting
--let $assert_count= 1
--source include/assert_grep.inc
--remove_file $MYSQL_TMP_DIR/master.err

--echo # Part 2
# Restore source's local manifest file to start it with keyring
--source include/keyring_tests/helper/instance_restore_manifest.inc

# Restart the source loading the keyring
--echo Starting the source with "--binlog_encryption=ON" and keyring installed
--let $rpl_server_number= 1
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --binlog_encryption=ON
--let $rpl_server_error= 0
--let $rpl_omit_print_server_parameters= 1
--source include/rpl/start_server.inc
--connection master

--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--source include/assert.inc

--let $assert_text= 2nd binary log is encrypted on source
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 2]" = "Yes"
--source include/assert.inc

--let $after_master_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $rpl_log_file=$MASTER_DATADIR$after_master_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=Binary log is encrypted using 1st master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_1"
--source include/assert.inc

--let $assert_text=Binary log rotated as expected at source startup
--let $assert_cond= RIGHT("$after_master_file", 6) = "000002"
--source include/assert.inc

# Next server startup will rely on this set persist
SET PERSIST binlog_encryption = ON;

--echo Restart the source with keyring installed
--let $rpl_server_number= 1
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT
--let $rpl_server_error= 0
--let $rpl_omit_print_server_parameters= 1
--source include/rpl/restart_server.inc

--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--source include/assert.inc

--let $assert_text= 3rd binary log is encrypted on source
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 3]" = "Yes"
--source include/assert.inc

--let $after_master_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $rpl_log_file=$MASTER_DATADIR$after_master_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=Binary log is encrypted using 1st master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_1"
--source include/assert.inc

--let $assert_text=Binary log rotated as expected at source startup
--let $assert_cond= RIGHT("$after_master_file", 6) = "000003"
--source include/assert.inc

# Restart the replica loading the keyring
--source include/rpl/connection_replica.inc
--let $assert_text= 1st binary log is not encrypted on replica
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 1]" = "No"
--source include/assert.inc

--let $relay_log_base=`SELECT @@GLOBAL.relay_log`
--let $relay_log_suffix=.000001
--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$relay_log_suffix
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=Relay log is not encrypted for $relay_log_base$relay_log_suffix
--let $assert_cond= "$rpl_encryption_key_id" = "None"
--source include/assert.inc

--echo Restart the replica with keyring installed
--let $rpl_server_number= 2
--let $rpl_server_parameters= $RPL_PLUGIN_DIR_OPT --skip-replica-start
--let $rpl_server_error= 0
--let $rpl_omit_print_server_parameters= 1
--source include/rpl/restart_server.inc

--let $assert_text=binlog_encryption option shall be OFF
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "OFF"
--source include/assert.inc

--let $assert_text= 2nd binary log is not encrypted on replica
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 2]" = "No"
--source include/assert.inc

--let $relay_log_suffix=.000002
--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$relay_log_suffix
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=Relay log is not encrypted for $relay_log_base$relay_log_suffix
--let $assert_cond= "$rpl_encryption_key_id" = "None"
--source include/assert.inc

SET GLOBAL binlog_encryption = ON;

--let $assert_text=binlog_encryption option shall be ON
--let $option_value = `SELECT variable_value FROM performance_schema.global_variables WHERE variable_name = "binlog_encryption"`
--let $assert_cond= "$option_value" = "ON"
--source include/assert.inc

--let $assert_text= 3rd binary log is encrypted on replica
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 3]" = "Yes"
--source include/assert.inc

--let $after_master_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $assert_text=Binary log rotated as expected at server startup
--let $assert_cond= RIGHT("$after_master_file", 6) = "000003"
--source include/assert.inc
--let $rpl_log_file=$SLAVE_DATADIR$after_master_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=New binary log is encrypted using 1st master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_1"
--source include/assert.inc
--let $relay_log_suffix=.000004
--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$relay_log_suffix
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=Relay log rotated as expected at server startup
--let $assert_cond= "$rpl_encryption_key_id" = "Error: unable to open the file"
--source include/assert.inc
--let $relay_log_suffix=.000003
--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$relay_log_suffix
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=Relay log is encrypted for $relay_log_base$relay_log_suffix
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_1"
--source include/assert.inc

# Verify that 'ALTER INSTANCE ROTATE BINLOG MASTER KEY' rotates
# the maskter key, which is used to encrypt new created relay
# logs and re-encrypt previous encrypted relay logs.
--let $debug_point=verify_unusable_encryption_keys_are_purged
--source include/add_debug_point.inc
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
--source include/remove_debug_point.inc

--let $relay_log_suffix=.000003
--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$relay_log_suffix
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=The third relay log is re-encrypted using second master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc

--let $relay_log_suffix=.000004
--let $rpl_log_file=$SLAVE_DATADIR$relay_log_base$relay_log_suffix
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=The after relay log is encrypted using second master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc

--echo # Part 3
--source include/rpl/connection_source.inc
# Slave can replicate from encrypted binary log file
CREATE TABLE t1 (c1 INT PRIMARY KEY);
INSERT INTO t1 (c1) VALUES (1), (2), (3);
INSERT INTO t1 (c1) VALUES (4), (5), (6);
--source include/rpl/connection_replica.inc
--source include/rpl/start_replica.inc
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc
--let $gtid_mode= `SELECT @@GLOBAL.gtid_mode`
--let $AUTO_POSITION=1
if ($gtid_mode == 'OFF') {
  --let $recover_master_file=$_saved_file
  --let $recover_master_pos=$_saved_pos
  --let $AUTO_POSITION=0
}

--echo # Part 4
--source include/rpl/connection_replica.inc
# Simulate keyring removal
# 1. backup existing data
--let KEYRING_FILE_PATH = $REPLICA_KEYRING_FILE_PATH
--source include/keyring_tests/helper/local_keyring_file_backup.inc
# 2. Reload keyring
ALTER INSTANCE RELOAD KEYRING;
# 3. Lock keyring to block WRITE operations
SET GLOBAL keyring_operations = OFF;
--source include/rpl/connection_source.inc
--source include/rpl/sync_to_replica.inc

# Make SQL thread to read next relay log file
FLUSH LOGS;
# The binary log shall rotate and is still encrypted
--let $assert_text= 5th binary log is encrypted on replica
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 5]" = "Yes"
--source include/assert.inc
--let $encrypted_master_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $assert_text=Binary log rotated as expected by the FLUSH LOGS
--let $assert_cond=RIGHT("$encrypted_master_file", 6) = "000005"
--source include/assert.inc
--let $rpl_log_file=$SLAVE_DATADIR$encrypted_master_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=New binary log is still encrypted
--let $assert_cond=RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc
--let $slave_sql_errno= convert_error(ER_REPLICA_RELAY_LOG_READ_FAILURE)
--source include/rpl/wait_for_applier_error.inc
--source include/rpl/stop_receiver.inc

RESET REPLICA ALL;
# A new relay log file is always created for the default channel
--let $encrypted_relay_file=`SELECT CONCAT(@@GLOBAL.relay_log,'.000001')`
--let $rpl_log_file=$SLAVE_DATADIR$encrypted_relay_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=New relay log is still encrypted
--let $assert_cond=RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc

# The server is still able to encrypt new relay log files
--replace_result $MASTER_MYPORT MASTER_MYPORT
--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST='127.0.0.1', SOURCE_PORT=$MASTER_MYPORT, SOURCE_USER='root'
--let $encrypted_relay_file=`SELECT CONCAT(@@GLOBAL.relay_log,'.000001')`
--let $rpl_log_file=$SLAVE_DATADIR$encrypted_relay_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=New relay log is still encrypted
--let $assert_cond=RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc

# SQL thread is still not able to read encrypted relay log files
START REPLICA SQL_THREAD;
--let $slave_sql_errno= convert_error(ER_REPLICA_FATAL_ERROR)
--source include/rpl/wait_for_applier_error.inc

# Disable the option to not rely on keyring anymore
SET GLOBAL binlog_encryption = OFF;
# The binary log shall rotate and no more be encrypted
--let $assert_text= 6th binary log is not encrypted on replica
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 6]" = "No"
--source include/assert.inc
--let $disabled_master_file=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $assert_text=Binary log rotated as expected when disabling the option
--let $assert_cond=RIGHT("$disabled_master_file", 6) = "000006"
--source include/assert.inc
--let $rpl_log_file=$SLAVE_DATADIR$disabled_master_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=New binary log is not encrypted
--let $assert_cond="$rpl_encryption_key_id" = "None"
--source include/assert.inc
RESET REPLICA ALL;
# A new relay log file is always created for the default channel
--let $encrypted_relay_file=`SELECT CONCAT(@@GLOBAL.relay_log,'.000001')`
--let $rpl_log_file=$SLAVE_DATADIR$encrypted_relay_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=New relay log is not encrypted
--let $assert_cond="$rpl_encryption_key_id" = "None"
--source include/assert.inc
# Now, both replication threads shall work fine again
--replace_result $MASTER_MYPORT MASTER_MYPORT
--eval CHANGE REPLICATION SOURCE TO SOURCE_HOST='127.0.0.1', SOURCE_PORT=$MASTER_MYPORT, SOURCE_USER='root'
--let $encrypted_relay_file=`SELECT CONCAT(@@GLOBAL.relay_log,'.000001')`
--let $rpl_log_file=$SLAVE_DATADIR$encrypted_relay_file
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=New relay log is not encrypted
--let $assert_cond="$rpl_encryption_key_id" = "None"
--source include/assert.inc
if ($AUTO_POSITION) {
  --disable_query_log
  CHANGE REPLICATION SOURCE TO SOURCE_AUTO_POSITION=1;
  --enable_query_log
}
if (!$AUTO_POSITION) {
  --disable_query_log
  --eval CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE='$recover_master_file', SOURCE_LOG_POS=$recover_master_pos
  --enable_query_log
}

--source include/rpl/start_replica.inc

# Restore original file
--source include/keyring_tests/helper/local_keyring_file_restore.inc

--source include/rpl/connection_source.inc

--let $third_binary_log=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--echo third_binary_log: $third_binary_log
--let $rpl_log_file=$MASTER_DATADIR$third_binary_log
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=the third binary log is encrypted using first master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_1"
--source include/assert.inc

--echo # Part 5
--let $debug_point=verify_unusable_encryption_keys_are_purged
--source include/add_debug_point.inc
ALTER INSTANCE ROTATE BINLOG MASTER KEY;
--source include/remove_debug_point.inc
--let $assert_text=the fourth binary log is encrypted on source
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 4]" = "Yes"
--source include/assert.inc

--let $rpl_log_file=$MASTER_DATADIR$third_binary_log
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=the third binary log is re-encrypted using second master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc

--let $fourth_binary_log=query_get_value(SHOW BINARY LOG STATUS, File, 1)
--let $rpl_log_file=$MASTER_DATADIR$fourth_binary_log
--source include/rpl/get_log_encryption_key_id.inc
--let $assert_text=the fourth binary log is encrypted using second master key
--let $assert_cond= RIGHT("$rpl_encryption_key_id", 2) = "_2"
--source include/assert.inc

SET PERSIST binlog_encryption = OFF;
--let $assert_text= 5th binary log is not encrypted on source
--let $assert_cond= "[SHOW BINARY LOGS, Encrypted, 5]" = "No"
--source include/assert.inc

# Verify that we can not rotate binlog master key when
# 'binlog-encryption' is off.
--error ER_RPL_ENCRYPTION_CANNOT_ROTATE_BINLOG_MASTER_KEY
ALTER INSTANCE ROTATE BINLOG MASTER KEY;

# Cleanup
RESET PERSIST;
--source include/rpl/connection_source.inc
DROP TABLE t1;
--source include/rpl/sync_to_replica.inc
--source include/rpl/connection_source.inc
SET GLOBAL binlog_encryption= OFF;
--source include/rpl/connection_replica.inc
SET GLOBAL keyring_operations= ON;
