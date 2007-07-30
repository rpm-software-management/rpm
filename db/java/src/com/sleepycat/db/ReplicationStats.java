/*-
 * DO NOT EDIT: automatically built by dist/s_java_stat.
 *
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2002,2007 Oracle.  All rights reserved.
 */

package com.sleepycat.db;

public class ReplicationStats {
    // no public constructor
    /* package */ ReplicationStats() {}

    private int st_log_queued;
    public int getLogQueued() {
        return st_log_queued;
    }

    private int st_startup_complete;
    public int getStartupComplete() {
        return st_startup_complete;
    }

    private int st_status;
    public int getStatus() {
        return st_status;
    }

    private LogSequenceNumber st_next_lsn;
    public LogSequenceNumber getNextLsn() {
        return st_next_lsn;
    }

    private LogSequenceNumber st_waiting_lsn;
    public LogSequenceNumber getWaitingLsn() {
        return st_waiting_lsn;
    }

    private int st_next_pg;
    public int getNextPages() {
        return st_next_pg;
    }

    private int st_waiting_pg;
    public int getWaitingPages() {
        return st_waiting_pg;
    }

    private int st_dupmasters;
    public int getDupmasters() {
        return st_dupmasters;
    }

    private int st_env_id;
    public int getEnvId() {
        return st_env_id;
    }

    private int st_env_priority;
    public int getEnvPriority() {
        return st_env_priority;
    }

    private int st_bulk_fills;
    public int getBulkFills() {
        return st_bulk_fills;
    }

    private int st_bulk_overflows;
    public int getBulkOverflows() {
        return st_bulk_overflows;
    }

    private int st_bulk_records;
    public int getBulkRecords() {
        return st_bulk_records;
    }

    private int st_bulk_transfers;
    public int getBulkTransfers() {
        return st_bulk_transfers;
    }

    private int st_client_rerequests;
    public int getClientRerequests() {
        return st_client_rerequests;
    }

    private int st_client_svc_req;
    public int getClientSvcReq() {
        return st_client_svc_req;
    }

    private int st_client_svc_miss;
    public int getClientSvcMiss() {
        return st_client_svc_miss;
    }

    private int st_gen;
    public int getGen() {
        return st_gen;
    }

    private int st_egen;
    public int getEgen() {
        return st_egen;
    }

    private int st_log_duplicated;
    public int getLogDuplicated() {
        return st_log_duplicated;
    }

    private int st_log_queued_max;
    public int getLogQueuedMax() {
        return st_log_queued_max;
    }

    private int st_log_queued_total;
    public int getLogQueuedTotal() {
        return st_log_queued_total;
    }

    private int st_log_records;
    public int getLogRecords() {
        return st_log_records;
    }

    private int st_log_requested;
    public int getLogRequested() {
        return st_log_requested;
    }

    private int st_master;
    public int getMaster() {
        return st_master;
    }

    private int st_master_changes;
    public int getMasterChanges() {
        return st_master_changes;
    }

    private int st_msgs_badgen;
    public int getMsgsBadgen() {
        return st_msgs_badgen;
    }

    private int st_msgs_processed;
    public int getMsgsProcessed() {
        return st_msgs_processed;
    }

    private int st_msgs_recover;
    public int getMsgsRecover() {
        return st_msgs_recover;
    }

    private int st_msgs_send_failures;
    public int getMsgsSendFailures() {
        return st_msgs_send_failures;
    }

    private int st_msgs_sent;
    public int getMsgsSent() {
        return st_msgs_sent;
    }

    private int st_newsites;
    public int getNewsites() {
        return st_newsites;
    }

    private int st_nsites;
    public int getNumSites() {
        return st_nsites;
    }

    private int st_nthrottles;
    public int getNumThrottles() {
        return st_nthrottles;
    }

    private int st_outdated;
    public int getOutdated() {
        return st_outdated;
    }

    private int st_pg_duplicated;
    public int getPagesDuplicated() {
        return st_pg_duplicated;
    }

    private int st_pg_records;
    public int getPagesRecords() {
        return st_pg_records;
    }

    private int st_pg_requested;
    public int getPagesRequested() {
        return st_pg_requested;
    }

    private int st_txns_applied;
    public int getTxnsApplied() {
        return st_txns_applied;
    }

    private int st_startsync_delayed;
    public int getStartSyncDelayed() {
        return st_startsync_delayed;
    }

    private int st_elections;
    public int getElections() {
        return st_elections;
    }

    private int st_elections_won;
    public int getElectionsWon() {
        return st_elections_won;
    }

    private int st_election_cur_winner;
    public int getElectionCurWinner() {
        return st_election_cur_winner;
    }

    private int st_election_gen;
    public int getElectionGen() {
        return st_election_gen;
    }

    private LogSequenceNumber st_election_lsn;
    public LogSequenceNumber getElectionLsn() {
        return st_election_lsn;
    }

    private int st_election_nsites;
    public int getElectionNumSites() {
        return st_election_nsites;
    }

    private int st_election_nvotes;
    public int getElectionNumVotes() {
        return st_election_nvotes;
    }

    private int st_election_priority;
    public int getElectionPriority() {
        return st_election_priority;
    }

    private int st_election_status;
    public int getElectionStatus() {
        return st_election_status;
    }

    private int st_election_tiebreaker;
    public int getElectionTiebreaker() {
        return st_election_tiebreaker;
    }

    private int st_election_votes;
    public int getElectionVotes() {
        return st_election_votes;
    }

    private int st_election_sec;
    public int getElectionSec() {
        return st_election_sec;
    }

    private int st_election_usec;
    public int getElectionUsec() {
        return st_election_usec;
    }

    public String toString() {
        return "ReplicationStats:"
            + "\n  st_log_queued=" + st_log_queued
            + "\n  st_startup_complete=" + st_startup_complete
            + "\n  st_status=" + st_status
            + "\n  st_next_lsn=" + st_next_lsn
            + "\n  st_waiting_lsn=" + st_waiting_lsn
            + "\n  st_next_pg=" + st_next_pg
            + "\n  st_waiting_pg=" + st_waiting_pg
            + "\n  st_dupmasters=" + st_dupmasters
            + "\n  st_env_id=" + st_env_id
            + "\n  st_env_priority=" + st_env_priority
            + "\n  st_bulk_fills=" + st_bulk_fills
            + "\n  st_bulk_overflows=" + st_bulk_overflows
            + "\n  st_bulk_records=" + st_bulk_records
            + "\n  st_bulk_transfers=" + st_bulk_transfers
            + "\n  st_client_rerequests=" + st_client_rerequests
            + "\n  st_client_svc_req=" + st_client_svc_req
            + "\n  st_client_svc_miss=" + st_client_svc_miss
            + "\n  st_gen=" + st_gen
            + "\n  st_egen=" + st_egen
            + "\n  st_log_duplicated=" + st_log_duplicated
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
            + "\n  st_pg_duplicated=" + st_pg_duplicated
            + "\n  st_pg_records=" + st_pg_records
            + "\n  st_pg_requested=" + st_pg_requested
            + "\n  st_txns_applied=" + st_txns_applied
            + "\n  st_startsync_delayed=" + st_startsync_delayed
            + "\n  st_elections=" + st_elections
            + "\n  st_elections_won=" + st_elections_won
            + "\n  st_election_cur_winner=" + st_election_cur_winner
            + "\n  st_election_gen=" + st_election_gen
            + "\n  st_election_lsn=" + st_election_lsn
            + "\n  st_election_nsites=" + st_election_nsites
            + "\n  st_election_nvotes=" + st_election_nvotes
            + "\n  st_election_priority=" + st_election_priority
            + "\n  st_election_status=" + st_election_status
            + "\n  st_election_tiebreaker=" + st_election_tiebreaker
            + "\n  st_election_votes=" + st_election_votes
            + "\n  st_election_sec=" + st_election_sec
            + "\n  st_election_usec=" + st_election_usec
            ;
    }
}
