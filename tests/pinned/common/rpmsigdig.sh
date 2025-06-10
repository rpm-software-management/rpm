
for v in SHA256HEADER SHA1HEADER SIGMD5 PAYLOADSHA256 PAYLOADSHA256ALT PAYLOADSIZE PAYLOADSIZEALT; do
    runroot rpm -q --qf "${v}: %{${v}}\n" ${pkg}
done
runroot rpmkeys -Kv --nosignature ${pkg}
