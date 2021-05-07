---
date: 11 January 2001
section: 8
title: rpm2cpio
---

名前
====

rpm2cpio - RPM (RPM Package Manager)パッケージから cpio アーカイブを
抽出する

書式
====

**rpm2cpio** \[filename\]

説明
====

**rpm2cpio** は引数で指定される一つの .rpm ファイルを cpio
アーカイブ形式に変換し、 標準出力に出力する。 引数に \'-\'
が指定された場合には、標準入力から rpm を読み込む。

\
**rpm2cpio rpm-1.1-1.i386.rpm**\
**rpm2cpio - \< glint-1.0-1.i386.rpm**\
**rpm2cpio glint-1.0-1.i386.rpm \| cpio -dium**

関連項目
========

*rpm*(8)

著者
====

    Erik Troan <ewt@redhat.com>
