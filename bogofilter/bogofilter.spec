# $Id$
# $Log$
# Revision 1.2  2002/09/17 06:39:49  adrian_otto
# Updating for release. Added NEWS items.
#
# Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
# 0.7.3 Base Source
#
Name: bogofilter
Version: 0.7.4
Release: 1
URL: http://bogofilter.sourceforge.net
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Applications/Internet
Summary: fast anti-spam filtering by Bayesian statistical analysis
Buildroot: %{_tmppath}/%{name}-%{version}-root

%description
Bogofilter is a Bayesian spam filter.  In its normal mode of
operation, it takes an email message or other text on standard input,
does a statistical check against lists of "good" and "bad" words, and
returns a status code indicating whether or not the message is spam.
Bogofilter is designed with fast algorithms  (including Berkeley DB system),
coded directly in C, and tuned for speed, so it can be used for production
by sites that process a lot of mail.

%prep
%setup
./configure --prefix=/usr

%build
make

%install
rm -rf ${RPM_BUILD_ROOT}/*
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
make DESTDIR=${RPM_BUILD_ROOT} install
gzip $RPM_BUILD_ROOT/%{_mandir}/*/*.?


%files
%{_mandir}/man1/bogofilter.1.gz
%{_bindir}/bogofilter
%doc README COPYING

%changelog
* Tue Sep 17 02:29:48 EDT 2002
- changed version to 0.7.4 after a number of bug fixes and feature additions
  see NEWS file for more information.

* Sat Sep 14 15:03:20 PDT 2002
- changed version to 0.7.3 and added "./configure --prefix=/usr" line

* Wed Aug 28 2002 Gyepi Sam <gyepi@praxis-sw.com>
- Switched from Judy library to Berkeley DB.
-parameterized version number. One less thing to remember
 
* Mon Aug 19 2002 Eric S. Raymond <esr@snark.thyrsus.com>
- Initial build.


