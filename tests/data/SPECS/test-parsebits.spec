Name: test-parsebits
Version: v1
Release: r1

Summary: a test of parseBits behaviour
License: GPLv2+
BuildArch: noarch

%{?extra:%{extra}}
Requires: good.0.1
Requires(): good.0.2 = v0
Requires( ): good.0.3
Requires(	 ): good.0.4 = v0
Requires(interp): good.1.1 > v1
Requires(interp ): good.1.2
Requires(interp	 ): good.1.3 > v1
Requires( interp): good.1.4
Requires(	 interp): good.1.5 > v1
Requires(preun): good.2.1 >= v2
Requires(pre): good.3.1 < v3
Requires(postun): good.4.1 <= v4
Requires(post): good.5.1
Requires(rpmlib): good.6.1 = v6
Requires(verify): good.7.1 > v7
Requires(pretrans): good.8.1 >= v8
Requires(posttrans): good.9.1 < v9
Requires(pre,postun): good.34.1 <= v34
Requires(pre ,postun): good.34.2
Requires(pre	 ,postun): good.34.3 <= v34
Requires(pre, postun): good.34.4
Requires(pre,	 postun): good.34.5 <= v34
Requires(interp,preun,pre,postun,post,rpmlib,verify,pretrans,posttrans): good.19.1

%description
This is %summary.

%files
