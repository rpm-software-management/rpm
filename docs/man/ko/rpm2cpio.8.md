---
date: 1995년 9월 15일
section: 8
title: rpm
---

이름
====

rpm2cpio - 레드햇 (RPM) 패키지를 cpio 화일로 변환

개요
====

**rpm2cpio** \[filename\]

설명
====

**rpm2cpio** 는 .rpm 화일명을 하나 받아들여서 표준출력으로 cpio
저장화일을 출력한다. 만약 아무런 전달인수가 없다면 표준입력으로부터 rpm
화일을 받아들인다.

\
***rpm2cpio rpm-1.1-1.i386.rpm***\
***rpm2cpio \< glint-1.0-1.i386.rpm***\
***rpm2cpio glint-1.0-1.i386.rpm \| cpio -dium***

참고
====

*glint*(8)*,* *rpm*(8)

저자
====

    Erik Troan <ewt@redhat.com>

번역자
======

\
이 만 용 **\<geoman\@nownuri.nowcom.co.kr\>**\
** \<freeyong\@soback.kornet.nm.kr\>**
