#ifndef BISON_LDSCRIPT_H
# define BISON_LDSCRIPT_H

#ifndef YYSTYPE
typedef union {
  uintmax_t num;
  enum expression_tag op;
  char *str;
  struct expression *expr;
  struct input_section_name *sectionname;
  struct filemask_section_name *filemask_section_name;
  struct input_rule *input_rule;
  struct output_rule *output_rule;
  struct assignment *assignment;
  struct filename_list *filename_list;
  struct version *version;
  struct id_list *id_list;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	kADD_OP	257
# define	kALIGN	258
# define	kENTRY	259
# define	kEXCLUDE_FILE	260
# define	kFILENAME	261
# define	kGLOBAL	262
# define	kGROUP	263
# define	kID	264
# define	kINPUT	265
# define	kINTERP	266
# define	kKEEP	267
# define	kLOCAL	268
# define	kMODE	269
# define	kMUL_OP	270
# define	kNUM	271
# define	kPAGESIZE	272
# define	kPROVIDE	273
# define	kSEARCH_DIR	274
# define	kSEGMENT	275
# define	kSIZEOF_HEADERS	276
# define	kSORT	277
# define	kVERSION	278
# define	kVERSION_SCRIPT	279
# define	ADD_OP	280
# define	MUL_OP	281


extern YYSTYPE ldlval;

#endif /* not BISON_LDSCRIPT_H */
