
MODULE = rpm		PACKAGE = rpmTransactionSet		PREFIX = Transaction

int
TransactionAdd(trans, header)
  rpmTransactionSet trans
  Header header
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

int
TransactionDepCheck(trans)
  rpmTransactionSet trans
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

int
TransactionRun(trans)
  rpmTransactionSet trans
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

rpmTransactionSet
TransactionOrder(trans)
  rpmTransactionSet trans
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

int
TransactionDealloc(trans)
  rpmTransactionSet trans
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

int
TransactionGetAttr(trans, name)
  rpmTransactionSet trans
  const char * name
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

int 
TransactionSetAttr(trans, name, val)
  rpmTransactionSet trans
  const char * name
  void * val
  CODE:
	ST(0) = sv_newmortal();
	ST(0) = &PL_sv_undef;

