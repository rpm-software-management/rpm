/*
 *  DO NOT EDIT: automatically built by dist/s_java_stat.
 */
package com.sleepycat.db;

/**
 *  The DbRepStat object is used to return replication subsystem
 *  statistics.</p>
 */
public class DbRepStat {
    /**
     *  The current replication mode. Set to {@link
     *  com.sleepycat.db.Db#DB_REP_MASTER Db.DB_REP_MASTER} if the
     *  environment is a replication master, {@link
     *  com.sleepycat.db.Db#DB_REP_CLIENT Db.DB_REP_CLIENT} if the
     *  environment is a replication client, {@link
     *  com.sleepycat.db.Db#DB_REP_LOGSONLY Db.DB_REP_LOGSONLY} if the
     *  environment is a log-files-only replica, or 0 if replication
     *  is not configured.
     *</ul>
     *
     */
    public int st_status;
    /**
     *  In replication environments configured as masters, the next
     *  LSN expected. In replication environments configured as
     *  clients, the next LSN to be used.
     *</ul>
     *
     */
    public DbLsn st_next_lsn;
    /**
     *  The LSN of the first log record we have after missing log
     *  records being waited for, or 0 if no log records are currently
     *  missing.
     *</ul>
     *
     */
    public DbLsn st_waiting_lsn;
    /**
     *  The number of duplicate master conditions detected.
     *</ul>
     *
     */
    public int st_dupmasters;
    /**
     *  The current environment ID.
     *</ul>
     *
     */
    public int st_env_id;
    /**
     *  The current environment priority.
     *</ul>
     *
     */
    public int st_env_priority;
    /**
     *  The current generation number.
     *</ul>
     *
     */
    public int st_gen;
    /**
     *  The site is currently in client recovery. When this field is
     *  set, LSN values are not authoritative.
     *</ul>
     *
     */
    public int st_in_recovery;
    /**
     *  The number of duplicate log records received.
     *</ul>
     *
     */
    public int st_log_duplicated;
    /**
     *  The number of log records currently queued.
     *</ul>
     *
     */
    public int st_log_queued;
    /**
     *  The maximum number of log records ever queued at once.
     *</ul>
     *
     */
    public int st_log_queued_max;
    /**
     *  The total number of log records queued.
     *</ul>
     *
     */
    public int st_log_queued_total;
    /**
     *  The number of log records received and appended to the log.
     *
     *</ul>
     *
     */
    public int st_log_records;
    /**
     *  The number of log records missed and requested.
     *</ul>
     *
     */
    public int st_log_requested;
    /**
     *  The current master environment ID.
     *</ul>
     *
     */
    public int st_master;
    /**
     *  The number of times the master has changed.
     *</ul>
     *
     */
    public int st_master_changes;
    /**
     *  The number of messages received with a bad generation number.
     *
     *</ul>
     *
     */
    public int st_msgs_badgen;
    /**
     *  The number of messages received and processed.
     *</ul>
     *
     */
    public int st_msgs_processed;
    /**
     *  The number of messages ignored due to pending recovery.
     *</ul>
     *
     */
    public int st_msgs_recover;
    /**
     *  The number of failed message sends.
     *</ul>
     *
     */
    public int st_msgs_send_failures;
    /**
     *  The number of messages sent.
     *</ul>
     *
     */
    public int st_msgs_sent;
    /**
     *  The number of new site messages received.
     *</ul>
     *
     */
    public int st_newsites;
    /**
     *  The number of sites believed to be in the replication group.
     *
     *</ul>
     *
     */
    public int st_nsites;
    /**
     *  Transmission limited. This indicates the number of times that
     *  data transmission was stopped to limit the amount of data sent
     *  in response to a single call to {@link
     *  com.sleepycat.db.DbEnv#replicationProcessMessage
     *  DbEnv.replicationProcessMessage}.
     *</ul>
     *
     */
    public int st_nthrottles;
    /**
     *  The number of outdated conditions detected.
     *</ul>
     *
     */
    public int st_outdated;
    /**
     *  The number of transactions applied.
     *</ul>
     *
     */
    public int st_txns_applied;
    /**
     *  The number of elections held.
     *</ul>
     *
     */
    public int st_elections;
    /**
     *  The number of elections won.
     *</ul>
     *
     */
    public int st_elections_won;
    /**
     *  The election winner.
     *</ul>
     *
     */
    public int st_election_cur_winner;
    /**
     *  The election generation number.
     *</ul>
     *
     */
    public int st_election_gen;
    /**
     *  The maximum LSN of election winner.
     *</ul>
     *
     */
    public DbLsn st_election_lsn;
    /**
     *  The number sites expected to participate in elections.
     *</ul>
     *
     */
    public int st_election_nsites;
    /**
     *  The election priority.
     *</ul>
     *
     */
    public int st_election_priority;
    /**
     *  The current election phase (0 if no election is in progress).
     *
     *</ul>
     *
     */
    public int st_election_status;
    /**
     *  The election tiebreaker value.
     *</ul>
     *
     */
    public int st_election_tiebreaker;
    /**
     *  The votes received this election round.
     *</ul>
     *
     */
    public int st_election_votes;


    /**
     *  Provide a string representation of all the fields contained
     *  within this class.
     *
     * @return    The string representation.
     */
    public String toString() {
        return "DbRepStat:"
                + "\n  st_status=" + st_status
                + "\n  st_next_lsn=" + st_next_lsn
                + "\n  st_waiting_lsn=" + st_waiting_lsn
                + "\n  st_dupmasters=" + st_dupmasters
                + "\n  st_env_id=" + st_env_id
                + "\n  st_env_priority=" + st_env_priority
                + "\n  st_gen=" + st_gen
                + "\n  st_in_recovery=" + st_in_recovery
                + "\n  st_log_duplicated=" + st_log_duplicated
                + "\n  st_log_queued=" + st_log_queued
                + "\n  st_log_queued_max=" + st_log_queued_max
                + "\n  st_log_queued_total=" + st_log_queued_total
                + "\n  st_log_records=" + st_log_records
                + "\n  st_log_requested=" + st_log_requested
                + "\n  st_master=" + st_master
                + "\n  st_master_changes=" + st_master_changes
                + "\n  st_msgs_badgen=" + st_msgs_badgen
                + "\n  st_msgs_processed=" + st_msgs_processed
                + "\n  st_msgs_recover=" + st_msgs_recover
                + "\n  st_msgs_send_failures=" + st_msgs_send_failures
                + "\n  st_msgs_sent=" + st_msgs_sent
                + "\n  st_newsites=" + st_newsites
                + "\n  st_nsites=" + st_nsites
                + "\n  st_nthrottles=" + st_nthrottles
                + "\n  st_outdated=" + st_outdated
                + "\n  st_txns_applied=" + st_txns_applied
                + "\n  st_elections=" + st_elections
                + "\n  st_elections_won=" + st_elections_won
                + "\n  st_election_cur_winner=" + st_election_cur_winner
                + "\n  st_election_gen=" + st_election_gen
                + "\n  st_election_lsn=" + st_election_lsn
                + "\n  st_election_nsites=" + st_election_nsites
                + "\n  st_election_priority=" + st_election_priority
                + "\n  st_election_status=" + st_election_status
                + "\n  st_election_tiebreaker=" + st_election_tiebreaker
                + "\n  st_election_votes=" + st_election_votes
                ;
    }
}
// end of DbRepStat.java
