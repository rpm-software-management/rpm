#ifndef __TETJ_H
#define __TETJ_H

/* Interface for generating TET like report journals
 *
 * This intended to be a simple way of creating journals which
 * can be analysed using standard TET journal tools without
 * having to compile or link against the TET libraries.
 * 
 * (C) Copyright 2002 The Free Standards Group Inc
 *
 * 2002/03/19 Chris Yeoh, IBM
 *
 * This is Revision: 1.2 
 * 
 * Log: tetj.h,v 
 * Revision 1.2  2002/04/29 03:54:50  cyeoh
 * Adds function to add end marker for test purposes
 * Adds IC start markers
 *
 * Revision 1.1  2002/03/19 06:07:49  cyeoh
 * Adds simple interface for generating TET journals
 *
 *
 */

/* Handle for journal functions */
struct tetj_handle;

enum testcase_results
{
  TETJ_PASS,
  TETJ_FAIL,
  TETJ_UNRESOLVED,
  TETJ_NOTINUSE,
  TETJ_UNSUPPORTED,
  TETJ_UNTESTED,
  TETJ_UNINITIATED,
  TETJ_UNREPORTED,
  TETJ_WARNING = 101,
  TETJ_FIP,
  TETJ_NOTIMP,
  TETJ_UNAPPROVE
};

/* Open journal */
int tetj_start_journal(const char *pathname, struct tetj_handle **handle,
                       char *command_run);
/* Close journal */
int tetj_close_journal(struct tetj_handle *handle);

/* Add Config information */
void tetj_add_config(struct tetj_handle *handle, char *message);

/* Add controller error message */
void tetj_add_controller_error(struct tetj_handle *handle, char *message);

/* test case start */
void tetj_testcase_start(struct tetj_handle *handle, 
                         unsigned int activity, char *testcase, char *message);

/* test case end */
void tetj_testcase_end(struct tetj_handle *handle, 
                       unsigned int activity, char *testcase, char *message);

/* test purpose start */
void tetj_purpose_start(struct tetj_handle *handle,
                        unsigned int activity, unsigned int tpnumber, 
                        char *message);

/* test purpose end */
void tetj_purpose_end(struct tetj_handle *handle,
                      unsigned int activity, unsigned int tpnumber);

/* Set testcase result */
void tetj_result(struct tetj_handle *handle,
                 unsigned int activity, unsigned int tpnumber, 
                 enum testcase_results result);

/* testcase info */
void tetj_testcase_info(struct tetj_handle *handle,
                        unsigned int activity, unsigned int tpnumber,
                        unsigned int context, unsigned int block,
                        unsigned int sequence, char *message);

extern int tetj_activity_count;
extern int tetj_tp_count;

#endif
