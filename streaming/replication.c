//Includes
#include "rrdpush.h"
#include "collectors/plugins.d/pluginsd_parser.h"

static void replication_receiver_thread_cleanup(void *ptr);
static void replication_sender_thread_cleanup_callback(void *ptr);
//extern struct config stream_config;

// Thread Initialization
static void replication_state_init(REPLICATION_STATE *state){
    memset(state, 0, sizeof(*state));
    netdata_mutex_init(&state->mutex);
}

void replication_sender_init(struct sender_state *sender){
    if(!default_rrdpush_replication_enabled)
        return;
    if(!sender || !sender->host){
        error("%s: Host or host's sender state is not initialized! - Tx thread Initialization failed!", REPLICATION_MSG);
        return;
    }

    REPLICATION_STATE tx_replication;
    replication_state_init(&tx_replication);
    sender->replication = &tx_replication;
    sender->replication->enabled = default_rrdpush_replication_enabled;
    info("%s: Initialize Tx for host %s .", REPLICATION_MSG,sender->host->hostname);
}

static unsigned int replication_rd_config(struct receiver_state *rpt, struct config *stream_config)
{
    if(!default_rrdpush_replication_enabled)
        return default_rrdpush_replication_enabled;
    unsigned int rrdpush_replication_enable = default_rrdpush_replication_enabled;
    rrdpush_replication_enable = appconfig_get_boolean(stream_config, rpt->key, "enable replication", rrdpush_replication_enable);
    rrdpush_replication_enable = appconfig_get_boolean(stream_config, rpt->machine_guid, "enable replication", rrdpush_replication_enable);
    // Runtime replication enable status
    rrdpush_replication_enable = (default_rrdpush_replication_enabled && rrdpush_replication_enable && (rpt->stream_version >= VERSION_GAP_FILLING));

    return rrdpush_replication_enable;
}

void replication_receiver_init(struct receiver_state *receiver, struct config *stream_config)
{
    unsigned int rrdpush_replication_enable = replication_rd_config(receiver, stream_config);
    if(!rrdpush_replication_enable)
    {
        info("%s:  Could not initialize Rx replication thread. Replication is disabled or not supported!", REPLICATION_MSG);
        return;
    }
    REPLICATION_STATE rx_replication;
    replication_state_init(&rx_replication);
    receiver->replication = &rx_replication;
    receiver->replication->enabled = rrdpush_replication_enable;
    info("%s: Initialize Rx for host %s ", REPLICATION_MSG, receiver->host->hostname);
}

// Thread creation
void *rrdpush_replication_sender_thread(void *ptr) {
    struct sender_state *s = (struct sender_state *) ptr;
    // can read the config.
    // Add here the sender thread logic
    netdata_thread_cleanup_push(replication_sender_thread_cleanup_callback, s->host);
    // Add here the thread loop
    // for(;;) {
    //     // wait to connect
    //     // send hi
    //     // retrieve response
    // }
    // Closing thread
    netdata_thread_cleanup_pop(1);
    return NULL;
}

void replication_sender_thread_spawn(RRDHOST *host) {
    netdata_mutex_lock(&host->sender->replication->mutex);

    if(!host->sender->replication->spawned) {
        char tag[NETDATA_THREAD_TAG_MAX + 1];
        snprintfz(tag, NETDATA_THREAD_TAG_MAX, "REPLICATION_SENDER[%s]", host->hostname);

        if(netdata_thread_create(&host->sender->replication->thread, tag, NETDATA_THREAD_OPTION_JOINABLE, rrdpush_replication_sender_thread, (void *) host->sender))
            error("%s %s [send]: failed to create new thread for client.", REPLICATION_MSG, host->hostname);
        else
            host->sender->replication->spawned = 1;
    }
    netdata_mutex_unlock(&host->sender->replication->mutex);
}

void *rrdpush_replication_receiver_thread(void *ptr){
    netdata_thread_cleanup_push(replication_receiver_thread_cleanup, ptr);
    struct receiver_state *rpt = (struct receiver_state *)ptr;
    // Add here the receiver thread logic
    // Add here the thread loop
    // for(;;) {
    //     // wait to connect
    //     // send hi
    //     // retrieve response
    // }
    // Closing thread
    netdata_thread_cleanup_pop(1);
    return NULL;    
}

void replication_receiver_thread_spawn(RRDHOST *host){
    netdata_mutex_lock(&host->receiver->replication->mutex);

    if(!host->receiver->replication->spawned) {
        char tag[NETDATA_THREAD_TAG_MAX + 1];
        snprintfz(tag, NETDATA_THREAD_TAG_MAX, "REPLICATION_RECEIVER[%s]", host->hostname);

        if(netdata_thread_create(&host->sender->replication->thread, tag, NETDATA_THREAD_OPTION_JOINABLE, rrdpush_replication_receiver_thread, (void *) host->sender))
            error("%s %s [send]: failed to create new thread for client.", REPLICATION_MSG, host->hostname);
        else
            host->receiver->replication->spawned = 1;
    }
    netdata_mutex_unlock(&host->receiver->replication->mutex);
}

// Thread clean-up & destroy
static void replication_sender_thread_cleanup_callback(void *ptr) {
    RRDHOST *host = (RRDHOST *)ptr;

    netdata_mutex_lock(&host->sender->replication->mutex);

    info("%s %s [send]: sending thread cleans up...", REPLICATION_MSG, host->hostname);

    //close sender thread socket or/and pipe
    //rrdpush_sender_thread_close_socket(host);
    // clean the structures
    // follow the shutdown sequence with the sender thread from the rrdhost.c file

    if(!host->rrdpush_sender_join) {
        info("%s %s [send]: sending thread detaches itself.", REPLICATION_MSG, host->hostname);
        netdata_thread_detach(netdata_thread_self());
    }

    host->sender->replication->spawned = 0;

    info("%s %s [send]: sending thread now exits.", REPLICATION_MSG, host->hostname);

    netdata_mutex_unlock(&host->sender->replication->mutex);
}

static void replication_receiver_thread_cleanup(void *ptr)
{
    // follow the receiver clean-up
    // destroy the replication rx structs
    RRDHOST *host = ptr;
}

// Any join, start, stop, wait, etc thread function goes here.
void replication_sender_thread_stop(RRDHOST *host) {

    netdata_mutex_lock(&host->sender->replication->mutex);
    netdata_thread_t thr = 0;

    if(host->sender->replication->thread) {
        info("%s %s [send]: signaling sending thread to stop...", REPLICATION_MSG, host->hostname);

        // Check if this is necessary for replication thread?
        //signal the thread that we want to join it
        //host->rrdpush_sender_join = 1;

        // copy the thread id, so that we will be waiting for the right one
        // even if a new one has been spawn
        thr = host->sender->replication->thread;

        // signal it to cancel
        netdata_thread_cancel(host->sender->replication->thread);
    }

    netdata_mutex_unlock(&host->sender->replication->mutex);

    if(thr != 0) {
        info("%s %s [send]: waiting for the sending thread to stop...", REPLICATION_MSG, host->hostname);
        void *result;
        netdata_thread_join(thr, &result);
        info("%s %s [send]: sending thread has exited.", REPLICATION_MSG, host->hostname);
    }
}

// Memory Mode access
void collect_replication_gap_data(){
    // collection of gap data in cache/temporary structure
}

void update_memory_index(){
    //dbengine
    //other memory modes?
}

/* The receiver socket is blocking, perform a single read into a buffer so that we can reassemble lines for parsing.
 */
static int receiver_read(struct receiver_state *r, FILE *fp) {
#ifdef ENABLE_HTTPS
    if (r->ssl.conn && !r->ssl.flags) {
        ERR_clear_error();
        int desired = sizeof(r->read_buffer) - r->read_len - 1;
        int ret = SSL_read(r->ssl.conn, r->read_buffer + r->read_len, desired);
        if (ret > 0 ) {
            r->read_len += ret;
            return 0;
        }
        // Don't treat SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE differently on blocking socket
        u_long err;
        char buf[256];
        while ((err = ERR_get_error()) != 0) {
            ERR_error_string_n(err, buf, sizeof(buf));
            error("STREAM %s [receive from %s] ssl error: %s", r->hostname, r->client_ip, buf);
        }
        return 1;
    }
#endif
    if (!fgets(r->read_buffer, sizeof(r->read_buffer), fp))
        return 1;
    r->read_len = strlen(r->read_buffer);
    return 0;
}

/* Produce a full line if one exists, statefully return where we start next time.
 * When we hit the end of the buffer with a partial line move it to the beginning for the next fill.
 */
static char *receiver_next_line(struct receiver_state *r, int *pos) {
    int start = *pos, scan = *pos;
    if (scan >= r->read_len) {
        r->read_len = 0;
        return NULL;
    }
    while (scan < r->read_len && r->read_buffer[scan] != '\n')
        scan++;
    if (scan < r->read_len && r->read_buffer[scan] == '\n') {
        *pos = scan+1;
        r->read_buffer[scan] = 0;
        return &r->read_buffer[start];
    }
    memmove(r->read_buffer, &r->read_buffer[start], r->read_len - start);
    r->read_len -= start;
    return NULL;
}

// Replication parser & commands
size_t replication_parser(struct receiver_state *rpt, struct plugind *cd, FILE *fp) {
    // create or reuse the parser without interference between streaming and replication
    // support REP on/off/pause/ack
    // GAP
    size_t result;
    PARSER_USER_OBJECT *user = callocz(1, sizeof(*user));
    user->enabled = cd->enabled;
    user->host = rpt->host;
    user->opaque = rpt;
    user->cd = cd;
    user->trust_durations = 0;

    PARSER *parser = parser_init(rpt->host, user, fp, PARSER_INPUT_SPLIT);

    if (unlikely(!parser)) {
        error("Failed to initialize parser");
        cd->serial_failures++;
        freez(user);
        return 0;
    }

    parser->plugins_action->begin_action     = &pluginsd_begin_action;
    parser->plugins_action->flush_action     = &pluginsd_flush_action;
    parser->plugins_action->end_action       = &pluginsd_end_action;
    parser->plugins_action->disable_action   = &pluginsd_disable_action;
    parser->plugins_action->variable_action  = &pluginsd_variable_action;
    parser->plugins_action->dimension_action = &pluginsd_dimension_action;
    parser->plugins_action->label_action     = &pluginsd_label_action;
    parser->plugins_action->overwrite_action = &pluginsd_overwrite_action;
    parser->plugins_action->chart_action     = &pluginsd_chart_action;
    parser->plugins_action->set_action       = &pluginsd_set_action;
    parser->plugins_action->clabel_commit_action  = &pluginsd_clabel_commit_action;
    parser->plugins_action->clabel_action    = &pluginsd_clabel_action;

    user->parser = parser;

    do {
        if (receiver_read(rpt, fp))
            break;
        int pos = 0;
        char *line;
        while ((line = receiver_next_line(rpt, &pos))) {
            if (unlikely(netdata_exit || rpt->shutdown || parser_action(parser,  line)))
                goto done;
        }
        rpt->last_msg_t = now_realtime_sec();
    }
    while(!netdata_exit);
done:
    result= user->count;
    freez(user);
    parser_destroy(parser);
    return result;
}

// gap processing
// FSMs for replication protocol implementation
// REP on
// REP off
// REP pause/continue
// REP ack

// RDATA

// Replication FSM logic functions

