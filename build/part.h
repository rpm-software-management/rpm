#ifndef _PART_H_
#define _PART_H_

#define PART_NONE                0
#define PART_PREAMBLE            1
#define PART_PREP                2
#define PART_BUILD               3
#define PART_INSTALL             4
#define PART_CLEAN               5
#define PART_FILES               6
#define PART_PRE                 7
#define PART_POST                8
#define PART_PREUN               9
#define PART_POSTUN             10
#define PART_DESCRIPTION        11
#define PART_CHANGELOG          12
#define PART_TRIGGERIN          13
#define PART_TRIGGERUN          14
#define PART_VERIFYSCRIPT       15
#define PART_BUILDARCHITECTURES 16
#define PART_TRIGGERPOSTUN      17

int isPart(char *line);

#endif
