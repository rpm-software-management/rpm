#include "system.h"

#include <sys/stat.h>
#include <string>

#include <rpm/rpmcli.h>
#include <rpm/rpmio.h>
#include <rpm/rpmlog.h>

#include "signature.hh"
#include "cliutils.hh"

#include "debug.h"

enum rpmSectionType : uint8_t {
    RPM_INSPECT_NONE        = 0,
    RPM_INSPECT_LEAD        = 1 << 0,
    RPM_INSPECT_SIGNATURE   = 1 << 1,
    RPM_INSPECT_MAIN_HEADER = 1 << 2,
    RPM_INSPECT_PAYLOAD     = 1 << 3,
    RPM_INSPECT_ALL         = RPM_INSPECT_LEAD | RPM_INSPECT_SIGNATURE 
			    | RPM_INSPECT_MAIN_HEADER | RPM_INSPECT_PAYLOAD
};

struct rpmSection {
    size_t offset = 0;
    size_t size = 0;
    bool exists = false;
};

struct rpmPackageLayout {
    rpmSection lead;
    rpmSection signature;
    size_t padding = 0;
    rpmSection mainHeader;
    rpmSection payload;
};

static char *outputDir = nullptr;
static int dumpTargets = RPM_INSPECT_NONE;

static struct poptOption inspectorOptsTable[] = {
    { "lead",      'l', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &dumpTargets, RPM_INSPECT_LEAD,
	N_("Dump the lead section"), nullptr },
    { "signature", 's', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &dumpTargets, RPM_INSPECT_SIGNATURE,
	N_("Dump the signature section"), nullptr },
    { "mheader",   'm', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &dumpTargets, RPM_INSPECT_MAIN_HEADER,
	N_("Dump the main header section"), nullptr },
    { "payload",   'p', (POPT_ARG_VAL|POPT_ARGFLAG_OR), &dumpTargets, RPM_INSPECT_PAYLOAD,
	N_("Dump the payload section"), nullptr },
    { "outdir",    'o', POPT_ARG_STRING,                 &outputDir,   0,
	N_("Output directory for extracted sections"), "DIR" },
    POPT_TABLEEND
};

static struct poptOption optionsTable[] = {
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, inspectorOptsTable, 0,
	N_("Inspector options:"), NULL },
    { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"), NULL },
    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

static constexpr unsigned char RPM_LEAD_MAGIC[] = { 0xed, 0xab, 0xee, 0xdb };
static constexpr size_t RPM_LEAD_SIZE = 96;

struct FdCloser {
    FD_t fd;
    ~FdCloser() { if (fd) Fclose(fd); }
};

static int mapRpmSections(const char *filePath, rpmPackageLayout *layout)
{
    char *msg = nullptr;

    FD_t fd = Fopen(filePath, "r.ufdio");
    if (!fd || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, "rpminspector: %s: %s\n",
		filePath, Fstrerror(fd));
	if (fd) Fclose(fd);
	return 1;
    }
    FdCloser fdCloser{fd};

    layout->lead.size = RPM_LEAD_SIZE;

    unsigned char leadBuf[RPM_LEAD_SIZE];
    if (Fread(leadBuf, 1, RPM_LEAD_SIZE, fd) != RPM_LEAD_SIZE) {
	rpmlog(RPMLOG_ERR, "rpminspector: %s: failed to read lead\n", filePath);
	return 1;
    }
    if (memcmp(leadBuf, RPM_LEAD_MAGIC, sizeof(RPM_LEAD_MAGIC)) != 0) {
	rpmlog(RPMLOG_ERR, "rpminspector: %s: not an RPM package\n", filePath);
	return 1;
    }
    layout->lead.exists = true;

    layout->signature.offset = Ftell(fd);

    Header sigh = nullptr;
    if (rpmReadSignature(fd, &sigh, &msg) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, "rpminspector: %s: %s\n", filePath, msg);
	free(msg);
	return 1;
    }
    layout->signature.size = Ftell(fd) - layout->signature.offset;
    layout->padding = (8 - (headerSizeof(sigh, HEADER_MAGIC_YES) % 8)) % 8;
    headerFree(sigh);
    layout->signature.exists = true;

    layout->mainHeader.offset = Ftell(fd);

    Header h = headerRead(fd, HEADER_MAGIC_YES);
    if (!h) {
	rpmlog(RPMLOG_ERR, "rpminspector: %s: failed to read main header\n", filePath);
	return 1;
    }
    layout->mainHeader.size = headerSizeof(h, HEADER_MAGIC_YES);
    headerFree(h);
    layout->mainHeader.exists = true;

    layout->payload.offset = Ftell(fd);

    struct stat sb;
    if (fstat(Fileno(fd), &sb) != 0) {
	rpmlog(RPMLOG_ERR, "rpminspector: %s: can't stat file\n", filePath);
	return 1;
    }
    layout->payload.size = sb.st_size - layout->payload.offset;
    layout->payload.exists = true;

    return 0;
}

static int extractToFd(const char *filePath, const rpmSection &section, FD_t out)
{
    FD_t fd = Fopen(filePath, "r.ufdio");
    if (!fd || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, "rpminspector: %s: %s\n", filePath, Fstrerror(fd));
	if (fd) Fclose(fd);
	return 1;
    }

    Fseek(fd, section.offset, SEEK_SET);

    uint8_t buf[BUFSIZ];
    size_t remaining = section.size;
    while (remaining > 0) {
	size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
	ssize_t nread = Fread(buf, 1, chunk, fd);
	if (nread <= 0)
	    break;
	if (Fwrite(buf, 1, nread, out) != nread)
	    break;
	remaining -= nread;
    }

    Fclose(fd);
    return remaining != 0;
}

static int dissectPackage(const char *rpmFile)
{
    rpmPackageLayout layout = {};
    if (mapRpmSections(rpmFile, &layout))
	return 1;

    struct {
	uint8_t flag;
	rpmSection &section;
	const char *name;
    } sections[] = {
	{ RPM_INSPECT_LEAD,        layout.lead,       "lead" },
	{ RPM_INSPECT_SIGNATURE,   layout.signature,  "signature" },
	{ RPM_INSPECT_MAIN_HEADER, layout.mainHeader, "main_header" },
	{ RPM_INSPECT_PAYLOAD,     layout.payload,    "payload" },
    };

    if (outputDir)
	mkdir(outputDir, 0755);
    for (auto &s : sections) {
	if (!(dumpTargets & s.flag) || !s.section.exists)
	    continue;

	if (outputDir) {
	    std::string path = std::string(outputDir) + "/" + s.name;
	    FD_t out = Fopen(path.c_str(), "w.ufdio");
	    if (!out || Ferror(out)) {
		rpmlog(RPMLOG_ERR, "rpminspector: %s: %s\n", path.c_str(), Fstrerror(out));
		if (out) Fclose(out);
		continue;
	    }
	    extractToFd(rpmFile, s.section, out);
	    Fclose(out);
	} else {
	    FD_t out = fdDup(STDOUT_FILENO);
	    extractToFd(rpmFile, s.section, out);
	    Fclose(out);
	}
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ec = EXIT_FAILURE;
    poptContext optCon = rpmcliInit(argc, argv, optionsTable);

    const char *rpmFile = poptGetArg(optCon);
    if (!rpmFile) {
	printUsage(optCon, stderr, 0);
	goto exit;
    }

    if (dumpTargets == RPM_INSPECT_NONE) {
	if (!outputDir) {
	    rpmlog(RPMLOG_ERR,
		"rpminspector: no section selected and no --outdir given\n"
		"Select a single section for stdout, or use --outdir for multiple\n");
	    goto exit;
	}
	dumpTargets = RPM_INSPECT_ALL;
    }

    if (!outputDir && __builtin_popcount(dumpTargets) > 1) {
	rpmlog(RPMLOG_ERR,
	    "rpminspector: multiple sections selected without --outdir\n"
	    "Select a single section for stdout, or use --outdir for multiple\n");
	goto exit;
    }

    ec = dissectPackage(rpmFile);

exit:
    rpmcliFini(optCon);
    return RETVAL(ec);
}
