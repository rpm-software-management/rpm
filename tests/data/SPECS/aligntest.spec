Name:           aligntest
Version:        1.0
Release:        1
Summary:        Testing block-aligned payloads
License:        GPL
BuildArch:      noarch

%description
%{summary}

%install
mkdir -p %{buildroot}/opt/aligntest/sub
# A file larger than a filesystem block, so its content can be block-aligned.
seq 1 20000 > %{buildroot}/opt/aligntest/big.txt
# A sub-block file and a nested one, which keep the ordinary cpio padding.
echo hello > %{buildroot}/opt/aligntest/small.txt
seq 1 5000 > %{buildroot}/opt/aligntest/sub/nested.txt
# Non-regular payload members exercise the unaligned paths.
: > %{buildroot}/opt/aligntest/empty.txt
ln -s big.txt %{buildroot}/opt/aligntest/link
# A symlink whose target is longer than a small alignment: symlink content is
# never block-aligned (only regular files are), so this must survive a small
# _payload_alignment on the v6 stripped read path.
ln -s sub/nested.txt %{buildroot}/opt/aligntest/longlink
# A hard link to a large (aligned) file exercises the hardlink write path
# together with block-aligned content.
ln %{buildroot}/opt/aligntest/big.txt %{buildroot}/opt/aligntest/big.hardlink

%files
%dir /opt/aligntest/sub
/opt/aligntest/big.txt
/opt/aligntest/big.hardlink
/opt/aligntest/small.txt
/opt/aligntest/sub/nested.txt
/opt/aligntest/empty.txt
/opt/aligntest/link
/opt/aligntest/longlink
