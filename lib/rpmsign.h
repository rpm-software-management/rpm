#ifndef _RPMSIGN_H
#define _RPMSIGN_H

#include <rpm/argv.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmsign
 * Create/delete package signatures.
 * @param argv		array of package path arguments (NULL terminated)
 * @param deleting	adding or deleting signature(s)
 * @param passPhrase	passphrase (ignored when deleting)
 * @return		0 on success
 */
int rpmcliSign(ARGV_const_t argv, int deleting, const char *passPhrase);

#ifdef __cplusplus
}
#endif

#endif /* _RPMSIGN_H */
