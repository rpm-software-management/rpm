runroot rpmbuild -bb --quiet \
	--define "optflags -O2 -g" \
	--define "_target_platform noarch-linux" \
	--define "_binary_payload w.ufdio" \
	--define "_buildhost localhost" \
	--define "use_source_date_epoch_as_buildtime 1" \
	--define "source_date_epoch_from_changelog 1" \
	--define "clamp_mtime_to_source_date_epoch 1" \
	--define "_use_weak_usergroup_deps 0" \
	/data/SPECS/attrtest.spec
for v in SHA256HEADER SHA1HEADER SIGMD5 PAYLOADDIGEST PAYLOADDIGESTALT; do
    runroot rpm -q --qf "${v}: %{${v}}\n" /build/RPMS/noarch/attrtest-1.0-1.noarch.rpm
done
runroot rpmkeys -Kv /build/RPMS/noarch/attrtest-1.0-1.noarch.rpm
