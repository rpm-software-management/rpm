
for v in SHA256HEADER SHA1HEADER SIGMD5 PAYLOADDIGEST PAYLOADDIGESTALT PAYLOADSIZE PAYLOADSIZEALT; do
    runroot rpm -q --qf "${v}: %{${v}}\n" ${pkg}
done
runroot rpmkeys -Kv ${pkg}
