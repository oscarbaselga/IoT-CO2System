# Possible problem qith HTTPS certificates

Both 'http_key.pem' and 'http_cert.pem' are generated based on a CN (Common Name) to reference the service name. 
As there is no DNS server, the CN value is the IP assigned to the node. However, this IP can change because
it is not static, causing the certificate to stop working.

This problem has not been considerated as critical, since the aim of the HTTPS part is simply to train with this technology.

Anyway, a new certificate and a new key are generated with the following command:
```
 $ openssl req -newkey rsa:2048 -x509 -sha256 -days 3650 -nodes -out {CERT_NAME}.crt -keyout {KEY_NAME}.key
```
 
And they can be converted into PEM files with:
```
 $ openssl x509 -inform PEM -in {CERT_NAME}.crt > {CERT_NAME}.pem
 $ openssl rsa -in {KEY_NAME}.key -text > {KEY_NAME}.pem
```

*Actual CN value is IP 192.168.43.94* 

