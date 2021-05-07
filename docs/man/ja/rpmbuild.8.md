---
date: 09 June 2002
section: 8
title: RPMBUILD
---

名前
====

rpmbuils - RPM パッケージのビルド

書式
====

パッケージのビルド:
-------------------

**rpmbuild** {**-ba\|-bb\|-bp\|-bc\|-bi\|-bl\|-bs**}
\[**rpmbuild-options**\] *SPECFILE \...*

**rpmbuild** {**-ta\|-tb\|-tp\|-tc\|-ti\|-tl\|-ts**}
\[**rpmbuild-options**\] *TARBALL \...*

**rpmbuild** {**\--rebuild\|\--recompile**} *SOURCEPKG \...*

その他:
-------

**rpmbuild** **\--showrc**

rpmbuild のオプション
---------------------

\[**\--buildroot ***DIRECTORY*\] \[**\--clean**\] \[**\--nobuild**\]
\[**\--rmsource**\] \[**\--rmspec**\] \[**\--short-circuit**\]
\[**\--sign**\] \[**\--target ***PLATFORM*\]

説明
====

**rpmbuild**
は、バイナリパッケージとソースパッケージの両方のビルド(作成)に利用される。
**パッケージ** はファイルのアーカイブと、アーカイブされたファイルの
インストール・アンインストールに使われるメタデータから構成される。
メタデータは補助スクリプト、ファイル属性、
パッケージの説明に関する情報からなる。 **パッケージ** には 2 種類あり、
インストールするためのソフトウェアをカプセル化するのに使われるバイナリ
パッケージと、バイナリパッケージを作成するのに必要なレシピとソースコード
からなるソースパッケージとがある。

次の基本モードからいずれか一つを選択しなければならない:
**パッケージのビルド**、**tar アーカイブからのパッケージのビルド**、
**パッケージの再コンパイル**、**設定の表示**。

一般的なオプション
------------------

以下のオプションはすべてのモードで使用可能である。

**-?**, **\--help**

:   使い方を通常よりも詳しく表示する。

**\--version**

:   使用中の **rpm** のバージョン番号を 1 行で表示する。

**\--quiet**

:   表示をできるだけ少なくする。
    通常は、エラーメッセージだけが表示される。

**-v**

:   より多くの情報を表示する。
    通常は、ルーチンの進捗メッセージが表示される。

**-vv**

:   沢山の汚いデバッグ情報を表示する。

**\--rcfile*** FILELIST*

:   **rpm** は、コロン(\`:\')で区切られた *FILELIST*
    の各ファイルを設定情報として読み込む。 読み込みは *FILELIST*
    に指定された順番で行われる。 *FILELIST* のデフォルトは
    */usr/lib/rpm/rpmrc*:*/usr/lib/rpm/\<vendor\>/rpmrc*:*\~/.rpmrc*
    である。

**\--pipe*** CMD*

:   **rpm** コマンドの出力を *CMD* へパイプする。

**\--dbpath*** DIRECTORY*

:   データベースのパスとして、デフォルトの */var/lib/rpm* ではなく
    *DIRECTORY* を使う。

**\--root*** DIRECTORY*

:   すべての操作において、 *DIRECTORY*
    をルートとしたファイルシステムを使う。 つまり、 *DIRECTORY*
    内にあるデータベースが依存性のチェックに使用され、 *DIRECTORY* に
    chroot(2) した後で、すべてのスクリプト
    (例えば、パッケージインストール時の **%post**
    や、パッケージビルド時の **%prep** など) が実行される。

ビルドオプション
----------------

rpm のビルド・コマンドの一般的な形式は以下の通りである:

> **rpmbuild** **-b***STAGE***\|-t***STAGE* \[ **rpmbuild-options** \]
> *FILE \...*

パッケージのビルドに spec ファイルを使用するのであれば **-b** を、
**rpmbuild** が spec ファイルを使うために (圧縮されていることもある) tar
ファイルの 内部から使用する spec ファイルを探すのであれば **-t**
を、それぞれ引数に指定する。 最初の引数の後ろにある、次の文字 (*STAGE*)
はビルドとパッケージ化の段階を指定するのに使われ、
以下のいずれかが指定される (訳注: 以下のものは spec ファイル、すなわち
-b が指定された場合であり、 tar ファイルからビルドする場合は -ta, -tb,
\... となる)。

**-ba**

:   (%prep, %build, %install を実行した後に)
    バイナリパッケージとソースパッケージをビルドする。

**-bb**

:   (%prep, %build, %install を実行した後に)
    バイナリパッケージをビルドする。

**-bp**

:   spec ファイルから \"%prep\" 段階を実行する。
    通常、ソースを展開しパッチを適用することを意味する。

**-bc**

:   (%prep を実行した後に) spec ファイルから \"%build\" 段階を実行する。
    一般的には \"make\" と等価である。

**-bi**

:   (%prep, %build を実行した後に) spec ファイルから \"%install\"
    段階を実行する。 一般的には \"make install\" と等価である。

**-bl**

:   \"list check\" を実行する。 spec ファイルの \"%files\"
    セクションのマクロが展開され、
    各ファイルが存在するかの検証をするためのチェックが行われる。

**-bs**

:   ソースパッケージだけをビルドする。

さらに、以下のオプションが利用可能である:

**\--buildroot*** DIRECTORY*

:   パッケージビルド時に BuildRoot タグを *DIRECTORY*
    ディレクトリに上書きする。

**\--clean**

:   パッケージが作成された後にビルドツリーを削除する。

**\--nobuild**

:   何のビルドも実行しない。spec ファイルの検査を行う場合に便利である。

**\--rmsource**

:   ビルド後にソースを削除する (単独で使用してもよい。例: \"**rpmbuild
    \--rmsource foo.spec**\")。

**\--rmspec**

:   ビルド後に spec ファイルを削除する。 (単独で使用してもよい。例:
    \"**rpmbuild \--rmspec foo.spec**\")。

**\--short-circuit**

:   指定された段階へ直接すすむ(すなわち、指定された段階までの全ての段階が
    飛ばされる)。 **-bc** と **-bi** (訳注: **-tc** と **-ti**も)
    でのみ使用できる。

**\--sign**

:   パッケージに GPG 署名を埋め込む。
    この署名は、パッケージの出所と完全性を検証するのに用いることができる。
    設定の詳細については **rpm**(8) の「GPG 署名」の節を参照のこと。

**\--target*** PLATFORM*

:   パッケージビルド時に *PLATFORM* を **arch-vendor-os**
    と解釈し、それに応じてマクロ **%\_target**, **%\_target\_arch**,
    **%\_target\_os** を設定する。

ビルドと再コンパイルのオプション
--------------------------------

rpm を使ってビルドするには、他にも 2 つのやり方がある。

> **rpmbuild \--rebuild\|\--recompile*** SOURCEPKG \...*

この方法で起動された場合、 **rpmbuild**
は指定されたソースパッケージをインストールし、
準備、コンパイル、インストールを行う。 さらに、 **\--rebuild**
の場合、新たなバイナリパッケージをビルドする。ビルドか完了したら
ビルドディレクトリは (**\--clean** を指定した場合と同様に)削除され、
パッケージのソースと spec ファイルも削除される。

SHOWRC
------

コマンド

> **rpmbuild \--showrc**

は **rpmbuild** が使う設定ファイル、 *rpmrc* と *macros*
で現在セットされているオプションすべての値を表示する。

ファイル
========

rpmrc の設定
------------

    /usr/lib/rpm/rpmrc
    /usr/lib/rpm/<vendor>/rpmrc
    /etc/rpmrc
    ~/.rpmrc

マクロの設定
------------

    /usr/lib/rpm/macros
    /usr/lib/rpm/<vendor>/macros
    /etc/rpm/macros
    ~/.rpmmacros

データベース
------------

    /var/lib/rpm/Basenames
    /var/lib/rpm/Conflictname
    /var/lib/rpm/Dirnames
    /var/lib/rpm/Filemd5s
    /var/lib/rpm/Group
    /var/lib/rpm/Installtid
    /var/lib/rpm/Name
    /var/lib/rpm/Packages
    /var/lib/rpm/Providename
    /var/lib/rpm/Provideversion
    /var/lib/rpm/Pubkeys
    /var/lib/rpm/Removed
    /var/lib/rpm/Requirename
    /var/lib/rpm/Requireversion
    /var/lib/rpm/Sha1header
    /var/lib/rpm/Sigmd5
    /var/lib/rpm/Triggername

一時ファイル
------------

*/var/tmp/rpm\**

関連項目
========

**popt**(3), **rpm2cpio**(8), **gendiff**(1), **rpm**(8),


    http://www.rpm.org/

著者
====

    Marc Ewing <marc@redhat.com>
    Jeff Johnson <jbj@redhat.com>
    Erik Troan <ewt@redhat.com>
