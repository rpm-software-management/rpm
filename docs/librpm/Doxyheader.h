/*! \mainpage librpm API Documentation.
     This documents the librpm API as available to rpm itself and to the
     various depsolvers or language bindings that rely upon it.

    It enables to build tools for:
    - \link rpmbuild	creating \endlink,
    - \link rpmsign	signing \endlink,
    - \link rpmtd	querying \endlink,
    - or \link rpmts	(un)installing \endlink RPM packages.

    Thread-safety: in general, data and object-like elements in the rpm
    API are only safe to be accessed and disposed of from the originating
    thread. Some central resources have rudimentary protection to support
    concurrent access though:
    - rpm configuration and macros
    - rpmlog subsystem
    - rpm string pool objects
    - rpm keyring and public keys
 */
/** \defgroup buildsign Building & signing packages:
 *
 * @{
 */
/** \defgroup	rpmbuild	Build API.
    \brief API for building packages.
 */
/** \defgroup	signature	Signature Tags API.
    \brief List of signature tags.
 */
/** \defgroup	rpmsign	Signature API.
    \brief How to add or remove a signature from a package header.
 */
/** @}*/

/** \defgroup datatypes Data types:
 *
 * @{
 */
/** \defgroup	rpmtypes	RPM data types.
    \brief The abstract RPM data types.
 */
/** \defgroup	rpmstring	String Manipulation API.
    \brief String Manipulation API.
 */
/** \defgroup	rpmstrpool	String Pool API.
    \brief How to store strings in pools.
 */
/** \defgroup	rpmver		RPM version API.
    \brief Rpm version comparison API.
 */
/** @} */
/** \defgroup install (un)Installing packages:
 *
 * @{
 */
/** \defgroup	rpmds	Dependency Set API.
    \brief How to compare dependencies.
 */
/** \defgroup	rpmcallback	Callback signature & types.
    \brief The signature of function to register as callback and the cases where it can be called
 */
/** \defgroup	rpmts	Transaction Set API.
    \brief How to create, run & destroy a package transaction.
 */
/** \defgroup	rpmte	Transaction Element API.
    \brief How to retrieve information from a transaction element.
 */
/** \defgroup	rpmps	Problem Set API.
    \brief Problem Set API.
 */
/** \defgroup	rpmprob	Problem Element API.
    \brief Problem Element API.
 */
/** \defgroup	rpmvf	Verify API.
    \brief How to verify a package
 */
/** @} */

/** \defgroup	rpmfiles	File Info Set API.
    \brief File Info Set API.
 */
/** \defgroup	rpmfi	File Info Set Iterator API.
    \brief File Info Set Iterator API.
 */
/** \defgroup	rpmfc	File Classification API.
    \brief Structures and methods for build-time file classification
 */
/** \defgroup	rpmkeyring	RPM keyring API.
    \brief RPM keyring API.
 */
/** \defgroup	rpmmacro	Macro API.
    \brief Macro API.
 */
/** \defgroup	rpmlog	Logging API.
    \brief RPM Logging facilities.
 */
/** \defgroup	rpmpgp	OpenPGP API.
    \brief OpenPGP constants and structures from RFC-2440.
 */
/** \defgroup headquery Querying package headers:
 *
 * @{
 */
/** \defgroup	header	Header API.
    \brief How to manipulate package headers (which carries all information about a package).
 */
/** \defgroup	rpmtag	RPM Tag API.
    \brief Manipulating RPM tags (accessing values, types, ...)
 */
/** \defgroup	rpmtd	RPM Tag Data Container API.
    \brief How to retrieve data from package headers.
 */
/** @} */
/** \defgroup io I/O
 *
 * @{
 */
/** \defgroup	rpmdb	Database API.
    \brief Opening & accessing the RPM database
 */
/** \defgroup	rpmio	RPM IO API.
    \brief The RPM IO API (Fd_t is RPM equivalent to libc's FILE).
 */
/** \defgroup	rpmfileutil	File and Path Manipulation API.
    \brief File and path manipulation helper functions.
 */
/** \defgroup	rpmurl	URL Manipulation API.
    \brief A couple utils for URL Manipulation.
 */
/** \defgroup	rpmargv	Argument Manipulation API.
    \brief Argument Manipulation API.
 */
/** \defgroup	rpmcli	Command Line API.
    \brief Parsing RPM command line arguments.
 */
/** \defgroup	rpmsq	Signal Queue API.
    \brief Signal Queue API.
 */
/** \defgroup	rpmsw	Statistics API.
    \brief Statistics API.
 */
/** \defgroup	rpmrc	RPMRC.
    \brief Reading config files and getting some important configuration values.
 */
/** @} */
