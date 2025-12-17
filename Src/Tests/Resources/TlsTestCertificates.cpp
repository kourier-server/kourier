//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "TlsTestCertificates.h"
#include <QTemporaryFile>
#include <forward_list>
#include <memory>


namespace Kourier::TestResources
{

static TlsTestCertificateInfo rsa2048()
{
/*
 RSA 2048
 ========================================================
 Root CA key: openssl genrsa -out ca.key 2048
 CA Certificate: openssl req -x509 -new -key ca.key -sha256 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Certificate key: openssl genrsa -out cert.key 2048
 Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIIDxjCCAq6gAwIBAgIUXjppcsxhzhZFygc13nfMa6kXZx0wDQYJKoZIhvcNAQEL
BQAwcTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEkMCIGA1UEAwwbcnNhMjA0
OC1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzEzNTgyNloXDTM1MTAyNjEz
NTgyNlowcTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENp
dHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEkMCIGA1UEAwwbcnNh
MjA0OC1jYS5yb290LWNlcnRpZmljYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A
MIIBCgKCAQEAzwMex4N/WOgzC9aOt+Wc17FhZAg5pPUqvw54amhbjzzSKMhs+4dZ
PqSoeauFyOMMHMZEMibLjNPmERwOZ8tThcQSm7USVEdpMFrL6lgvVtjsNFu2mnV2
+cbzjJViOdO7f/hCHVPiVarj7FzM7WAasRA1/fNnMyUO1e56E8EDWIt+vamxdpE7
ddwZ3l4yOqKl/ex4Lm2ob5mCd6QGsoxmnFmQR3fue+TfindU1zgrYt/d8fgu3Ufe
kJ+6WyGHNEXP7lbVP5SQtwOtXNND1AZsuQfSVMtsDR3ROk+yLude5wWe/bp1rfMH
aN7oU/1j8U3qwSg3y/ilezFMfmxx1XrNPwIDAQABo1YwVDAdBgNVHQ4EFgQUzSv2
Rx23Pn2mNzz9vY1CDj4dULAwHwYDVR0jBBgwFoAUzSv2Rx23Pn2mNzz9vY1CDj4d
ULAwEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG9w0BAQsFAAOCAQEAU/ox5gvW
CUHzUSefiMsBamndP9ZkjMb824qOz1iQEsq9K3xRVWCqdmDUX5p89oiXpRIjfDhd
II5WFRn5SgLpk3bcblO48xWXcDgSurGf8qrz0/oWnxZr58y81THKVB9c/n4CsjEu
UzPiZ0iHt2Lg0fQRY6CGnEaSR92GMYuuVyckKOc2F0LE5uMHkCbjITPv6KUZhc73
JskK1/ANDiEAK60j7BsB4YixpTP85cgN0QQDkYurqje2oOzgCTpoMSLrVyWervpR
DZ739KO8tvHV3+KibY9GpGC6DfauHwPdrUCKUTcRwAxqVEmrDDHLxsNeMKrziOtm
maD1zaJPZitifQ==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCOdyJaNJb6fJRU
aFJ98oNT854Gtn2EnffmLkMGrScwjiTocvaXaWlqkeWPxjX9KLQqvB0CbIdlaSYV
/gR7mEGRFw9WVClWbHY/ZB3KL51z4t0PJrmHEGwCbmm3I9HfCCrvN616q9v4lODT
SyddP7Tu7Em2/P5j4w/kmSMMS0Gtd8MxHhV/0zfdtPiXYtSy/dKA3gS53KCKIy+d
VzJ8v4Qrk/QDYkPmuHIs1Q77bNWsgGHtFuJKApztBkvsGmOdQ0QIzeO/k+gH4mmW
rpsK1sL/yHoT7Q/hn2SX/85tbm4Om/EeNJBHIJ6H3MDnVezu9iUGXtMh41WgzVIr
d8FuxPKRAgMBAAECggEAIwM3b5juJAh7AdWTtH1l9jtwsnCmH3IdSzZCZcEnWQEO
JAyg+uw+qqDJnNCXUyzuPnR14rMegXdak1wW6PMt5gUXUV2kvScx1nl58sdJ+IGg
V4lqXj3UdbKCC67jdN9SgfEakwPqr0iTioo1Ve8CvhdXNIAv2weiT8ms+egC76kZ
7Hd3hGQVKtZzrWEDUbJ/o1JQHTUJekO43FHZngF9eQcbY6hnQUwNvYDPBuEWOF/U
SFKUEqPO+VFZWi6hZfMyCqerhlAz7HLtt76sSf/aoO1+JTQs1cmSKDsFNv3HPudD
CUnlYepINsu4QvHlvK1ys3gQ6NtOpWsurw2qV7nKWwKBgQDDRk3U+mu3Glv8arm2
JjZ0l88w2CJV7zaFnCHD/XYRaB4eCgN+f7FCjr81SC3OZmMquosxUG9+SlhD4Sgb
Jml3LGVIvEBWINZKIDVo11Fj8mU2cQTNBLCW1suzCUGNLWQ4k6dVZRX5aNsOgECw
oPwRt8A9GwRMv6rEMNjMOZIOZwKBgQC6xLeBSMhABPDq4l47C32CU61RV6LulLTn
vLWUZCtwvJnGIZ4cY2QusdtiEch6225yylRfiZgNki83gJqBjAlIyFKrLXtx2XXN
AkPCXeyoYjicZwE/CoLNsbxUQrXNoQVWUnpeat9GjqwjMXorN5ZA+PQTL93sCzOj
paEKqKTsRwKBgE0I6R+CAwhi77Lub4D6JjVsxiHgwfrgucOPyopE7VUEnA0Bqks3
GLjHE4tzhA6OucRbfxnfP3y024k7w9SiHK0U6If/K+pOXfs0JZ3Mg6FIOMF3aItw
tDFtX0Kr/h4xr/3Z0gOQM1EHGpPn6J1FhOuAb+grRlD4s+dd1ckLVSE1AoGAMB0L
NPf99lgPhELqiYVtQMyzonfUFmgirmwzztV7R2CesKbqZ1/HaCwTWxE/tz6eFl4/
HBmDHUPifAbaRrM/lQteGJDv8yVVLon90K2HkN00vj3e2VGo3rqBzKce1Kk9ib5X
nckkWbXZbHuLgGSihXxczDCRmAROukKp2OgXDHMCgYAckEN3e+U84taRpaeHL+g1
fgjkoGm+jDFliUupanYhiEjywr7VQHhjc2jh3scJAhn7l7T9ZyNhlb5t7MUD3/Fu
c2jvPP1ict8rpy1TEkik6R1ZhLKfR2wV9HWd6arQmLwBZTk4RZYt8PMr/+E/tw+/
erOavTePfjGQtYLGBlSxpw==
-----END PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIEKzCCAxOgAwIBAgIUUFpszAirfmr0AWc//BMrXaMkGn0wDQYJKoZIhvcNAQEL
BQAwcTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEkMCIGA1UEAwwbcnNhMjA0
OC1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzEzNTkyMloXDTM0MDMwNTEz
NTkyMlowaTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENp
dHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEcMBoGA1UEAwwTcnNh
MjA0OC1jZXJ0aWZpY2F0ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB
AI53Ilo0lvp8lFRoUn3yg1Pznga2fYSd9+YuQwatJzCOJOhy9pdpaWqR5Y/GNf0o
tCq8HQJsh2VpJhX+BHuYQZEXD1ZUKVZsdj9kHcovnXPi3Q8muYcQbAJuabcj0d8I
Ku83rXqr2/iU4NNLJ10/tO7sSbb8/mPjD+SZIwxLQa13wzEeFX/TN920+Jdi1LL9
0oDeBLncoIojL51XMny/hCuT9ANiQ+a4cizVDvts1ayAYe0W4koCnO0GS+waY51D
RAjN47+T6AfiaZaumwrWwv/IehPtD+GfZJf/zm1ubg6b8R40kEcgnofcwOdV7O72
JQZe0yHjVaDNUit3wW7E8pECAwEAAaOBwjCBvzB9BgNVHREEdjB0giEqLmlwdjRf
aXB2Nl9hZGRyZXNzZXMub25sb2NhbGhvc3SCH2lwdjRfaXB2Nl9hZGRyZXNzZXMu
b25sb2NhbGhvc3SHBH8KFDKHBH8KFDyHBH8KFEaHBH8KFFCHBH8KFFqHEAAAAAAA
AAAAAAAAAAAAAAEwHQYDVR0OBBYEFB+Zp0kv1Zev/dTJ6Lr5AbIVyJsPMB8GA1Ud
IwQYMBaAFM0r9kcdtz59pjc8/b2NQg4+HVCwMA0GCSqGSIb3DQEBCwUAA4IBAQCq
A6Ei2oMIZAX/dh5cMHxXcf98zmqBqlVWKbLNBqxYrX6gaIHLkYdqqZbalsF/Zcae
9jh+Qn8u3JPi/2W9W4FjaWHT5cvoajPe5f2oMT9tBgHpVYmicBZtRqXbSstb2tlN
6JVHXInnxT+xjP5Eyjf1sSjcFMj7PHBY7UmraToz/bQny5HsJYji+Bybb2RdmrGk
/F8mzOiRqulK0CBVnNf1jFG8YBeIHasyxN/rk9oscZzbMpIU1VH9b4aSsDjEioST
/cHh0yIlX2sX2DUNWWGkvwP5DD5acAgTWSUY9V8DxqdzGjA43+Zf73pQX+aW6BcP
W4TIC51VxHY0r4geMWL7
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo rsa2048Chain()
{
/*
 RSA 2048 with intermediate certificate
 ========================================================
 Root CA key: openssl genrsa -out ca.key 2048
 CA Certificate: openssl req -x509 -new -key ca.key -sha256 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Intermediate Certificate key: openssl genrsa -out intermediate-cert.key 2048
 Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-intermediate-certificate"
 Sign Intermediate Certificate: openssl x509 -req -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -extfile <(printf "basicConstraints=critical,CA:TRUE,pathlen:10") -days 3000 -sha256
 Certificate key: openssl genrsa -out leaf-cert.key 2048
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-leaf-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID0jCCArqgAwIBAgIUC4e+/A4QwE3ZebH+vSs48Yebs1IwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzE5NTkzMloXDTM1
MTAyNjE5NTkzMlowdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UE
AwwhcnNhMjA0OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEApVBpPDvHP/tEFcko7s4v3rwceXuQLlIRXnza
iL8lO9hvi+CR4O+pI0NBlGcHhci94Uhrjbys/D2Uudo2xmA9/1CDpTMkoTEf1F4o
gnR7M9of3t73+OED3HQWfKsM5TLX7iuBCwiVgvBre4iAlOv6MLCDD/usA0traj5o
2GWMF/7Rzd6yWjYqcRqh/Dp8cMcfllvzfCO2DYKLMQ2HifVqLdScE8/Wx+XXH7Iq
xxs6nVGl21c7xA/CKg47Hk/ni1nO0ts0/kYIn3NK3YFI5hWojEQme9ob+mCKvn/+
u051ZvP/qllIv00HC3eaeYjjpsQ84BXpm1y6jYQ3cJahfIyxuwIDAQABo1YwVDAd
BgNVHQ4EFgQUhiH3xqwrDuRClsUP2WmmDbpj51owHwYDVR0jBBgwFoAUhiH3xqwr
DuRClsUP2WmmDbpj51owEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG9w0BAQsF
AAOCAQEAb+l8j6OA25O4TmliYAMvaHCcLOawC72002g4tegsZJZEeCzxhWSdjzEd
JS8YfWOEDK+PrDGIx+rXTcdjBE/5infN5PgFo9cq79kWbc1oL5T9J07fgVxjAqG6
6oIr2wxqOBgehgBW5REgMQ/B4e1nIUT5myYJqeTARXJNbwHa+dNNrMe5toVUOcnS
Mxj+Q3tm1wVXSKw+6VIJlv5g8UORTlhBVfisY1P2e1DRm8YmMrApJGb7BAHdXumP
+LPPZEpQfWptUC+VL51zLLufjX8VCrKX33CwXVlJDuWvxjhymhpPa+V2dTllkGgH
F0W14xZofFh4UeweCarY5YGbAZi3og==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCbHygDWOtrw+wW
JkqC2dP+qXOvhnp+Oo+hCU6xgstcBn5AImi3aXUXkuKC7tJTd9l/j0FPkGJkKcDY
NaV3tqzbptppCX04DZ92wdwQTZxxT0xfSt54Jg7I4ZzPybRHtvSI5eNsv1CB4YkR
qTdUWYY8n/ss2dVKEgwwVEcFC+h0iLbxuFX7yyHgYxVvkCpfaM2WkcRmRIdvNmlW
z3TxJoiNKlIj/RFHv6jzxZD6I5sTzZkpKXwo8j5SuT7oxyfTIDShEfcvePKh2oL1
vVbCSE/FHlwegHGdhSJ5OcW5IjKTPlWQtKUgJnfZOtxUD8A5rVDwsyMjL8IJOoLV
F5wlsbCxAgMBAAECggEADL/EdR9OHLRPPLQhnvGHkpWbaXYJrpg3jR+2X7Ev75H6
YbZc7ujJnooMTdXnuW556hmnCi1ZY4FpqnDLobQvhCS0LLqCl88e0mQ1JwM0nrzK
unDtKejO1u8cCUPlqkM82TIzvTtYGDn7dFUjtANg8gNKtzFMGLi1Ab/bemxedZKq
GBx9TRfwosk5BeWzQZHpTx4qRDMRTXgX3D22mSU6BtVS8qQGMbQwM6htkU0fjXld
Dp126Hyhd8AFAT9TAgiIHlQi3je0keWq50Qwy3WpUavY3//5Pa3/avasqDBN1GlO
rUkrxNPnV4uklXiXopD+TiMH6YwdRsvoWnAv7dc2BQKBgQDXnuhxNp33f6CxKVjs
sCe9UdFhVILI8aeNrFOMpXhDscni7b9L6LkLDQYPUdPvw/860Xu+nBwlTnswc/Zg
fjQTLj6PN2GephJF0JNP1G4k46jdVGYfQ/CWUKdIf7GoFaGue4H95MJEZL21Qnne
ugusIm3aXjshTtMcexJnjDznAwKBgQC4K9vyMV6jjg9ZHYuUWK6zKRFfK6zbB3Lh
Wne3fcyRT3BBddGfI+Iaqzq0EMajUIXlLfjbWjGXN1fcHJ7Ecn2bEA0YbDCbZ6SW
Mwmsc/gzHm3ZWlMrUIHJBwd0/iSLFyK9bJ/WbVOvWq6bYR9jp02SyVv7/BISWvGW
Xq8diAXROwKBgBqTvbbmsoXlN1te61URSXSudw52KkC2eJ6f1RSK+M3vRiEwiNnE
b7qK+mo8/dCZ6gXH+GbAOz/l8o1AXef65lRO6OsiOmghtDLiyKWYW5M0dIYDdyr3
ZtpASr5G7xD2yZ0GIDm5TrC7ch67eOYZLIMlI0E9SDCM1Ly39sbIpGEdAoGAAxdZ
XbtOZ1hfEZPZX+gNJpyt2fcVs6dwWi/2inHmGRpIPBEKL62egewV1Cfj7aHQXYRk
BOqCopMfOmaQk6OIUC34ykwmlLUsogjBRM+9fr4oAdnuQ2/afdFMYr5RRrRgtOU5
2ZOEMBFJlPoQWM/aEXinvpcAhqZYH7n/HnPGBTkCgYAU9mTnpLXoeni65z5Jo/14
5/0y2XhRZUZxfuUaBGXcqGF+2q/tpTOf0rcvgwFl2NFP41fMSlkVlh1EgyhC1GXl
Uksa1UlZwpllfqifPLDZbKSNlLEKpnmZA6zAhDzNcfhghJK1xTZ6hekBzlWL2aot
/ahtO077EP81dinuw9ET+A==
-----END PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIEQTCCAymgAwIBAgIUAzdnsak8iLwryhWkRhb+hbH8iPIwDQYJKoZIhvcNAQEL
BQAwfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UEAwwmcnNhMjA0
OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwHhcNMjUxMjE3MjAwMDU0
WhcNMzQwMzA1MjAwMDU0WjB0MQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTEN
MAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMScw
JQYDVQQDDB5yc2EyMDQ4LWNoYWluLWxlYWYtY2VydGlmaWNhdGUwggEiMA0GCSqG
SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCbHygDWOtrw+wWJkqC2dP+qXOvhnp+Oo+h
CU6xgstcBn5AImi3aXUXkuKC7tJTd9l/j0FPkGJkKcDYNaV3tqzbptppCX04DZ92
wdwQTZxxT0xfSt54Jg7I4ZzPybRHtvSI5eNsv1CB4YkRqTdUWYY8n/ss2dVKEgww
VEcFC+h0iLbxuFX7yyHgYxVvkCpfaM2WkcRmRIdvNmlWz3TxJoiNKlIj/RFHv6jz
xZD6I5sTzZkpKXwo8j5SuT7oxyfTIDShEfcvePKh2oL1vVbCSE/FHlwegHGdhSJ5
OcW5IjKTPlWQtKUgJnfZOtxUD8A5rVDwsyMjL8IJOoLVF5wlsbCxAgMBAAGjgcIw
gb8wfQYDVR0RBHYwdIIhKi5pcHY0X2lwdjZfYWRkcmVzc2VzLm9ubG9jYWxob3N0
gh9pcHY0X2lwdjZfYWRkcmVzc2VzLm9ubG9jYWxob3N0hwR/ChQyhwR/ChQ8hwR/
ChRGhwR/ChRQhwR/ChRahxAAAAAAAAAAAAAAAAAAAAABMB0GA1UdDgQWBBS9Zy/R
ImbpQ26KfHQu+MQD2rmooDAfBgNVHSMEGDAWgBT/FoLOZKJVONhN+v5mI6n7CRu8
djANBgkqhkiG9w0BAQsFAAOCAQEAi9e6wAJzygdlHsgdY7fxToaOOzQmY51UTDqz
ZMVsLFDof7LxBe3RYcJJqRhOoDCuMK4wP7pxk2YcgbAzMf67rqK2+CLRD68us0iA
7b+ZmY6AV9HR2KLknX7Enibvbm7pqbIQzJkig3bXmW3WOnYFlqyhpkXSfs4v1TWq
D3FC7e5/nUX1ML3X2kXag+jl1GLpOS42fXl2aaK298yeTNYjlw/2iZgC+mKC2ZJr
jeB/3RMXJRqEnXVBAvk8DwudK5uyBaaCUxP1S6p9WHwyp4Dz1HuJPJTe6jUQlp9m
IVC1STm8S3PV0btjaDvXfdCVfcopReqUzfUYFe0OJDlbQCPHvA==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIID1zCCAr+gAwIBAgIUVNUE00kWCaK4ZafjGomX5y722y8wDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzIwMDAyOFoXDTM0
MDMwNTIwMDAyOFowfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UE
AwwmcnNhMjA0OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwggEiMA0G
CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDUxeoPdqB39dCM0JnmehVzgjpwiBBy
4U5IC8vwBBbuON/cACWrbAkW7PZUD/JVDf+Lv2Mqnn1A/EADCTA1l4LanNXmyOdA
0Np4YgM2A9ShCvZkgf5tA4ciqBH9H59AofO+p0ZlVpwgtaRkPb5OjHlCWrXN3fJw
H9rkjQTq9cAZ7C0j28DM6D5uGDb7d1DvrmErIF/OPitFZ2LpdoaoaE6DI5M8I34a
rBRT3AmMKYZ39pKJ5yEAlJ/fGxSbAhI7M8kgCQEYUunZeWSSny8RElqzXt1yuBR7
MHjzPYvUguJrIZ15Jil/VFqOYyvxaYqU4MI7OPd0GSbPWde6J/yHtrcvAgMBAAGj
VjBUMBIGA1UdEwEB/wQIMAYBAf8CAQowHQYDVR0OBBYEFP8Wgs5kolU42E36/mYj
qfsJG7x2MB8GA1UdIwQYMBaAFIYh98asKw7kQpbFD9lppg26Y+daMA0GCSqGSIb3
DQEBCwUAA4IBAQCDXFodfw11+iCdtabPhuBya3t8R91jOM7XfR1cTZ9b18TYTdXD
R6ak0WUyTHjriMGADxUXIsARY1CBjzWXOKC5EXEvx9EkwIBFds6XGP/yvY+GXDgB
nkpHJQt7utnHyOr87eewAdE4APLoykjA1jy7JRUYvwnNqoLzlpXpb8rmm7PP+Yu7
AFE8aYmRuJuJ19bemYeBGe+v8y/eZzrdPpHl1e0EShan0moauRVmv1KyXcw3/25U
IYGI7syhhhUxWp7QKaXIeISTHEOyUbxcG/KvKEczhJ50Kqg2NE0Z13pAYeLkSZVi
KhSdOkVJXU9RdNMcZQ0tzPWhrZww0I+1M18W
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo rsa2048_EncryptedPrivateKey()
{
/*
 As RSA 2048, but encrypting private key with password password, using
 the following command: openssl pkcs8 -topk8 -in rsa2048.key -out rsa2048-encrypted.key

 Root CA key: openssl genrsa -out ca.key 2048
 CA Certificate: openssl req -x509 -new -key ca.key -sha256 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-enc-pkey-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Certificate key: openssl genrsa -out cert.key 2048
 Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-enc-pkey-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in cert.key -out cert-encrypted.key

*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID2DCCAsCgAwIBAgIUIRvrZO7GNhjV9JAE7oX8vstfv/swDQYJKoZIhvcNAQEL
BQAwejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkcnNhMjA0
OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzE1Mjk1MVoX
DTM1MTAyNjE1Mjk1MVowejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTAL
BgNVBAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsG
A1UEAwwkcnNhMjA0OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMIIBIjAN
BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvN2dGEU41kyWJVguJpbOMN/BcNO7
u+FMyv7sD+jpIuPd/j0QYIUyzl6tI2qQT4aujm9mR+pyj8o14kG0vPglXGBwmmpY
GPb9HuTs4He0B8erJzveInSh5ERU6OucLk5FVyjy+9lU1DYyvcSomSX+J1KoribW
azvBmzdeb2p6U40BK80Cg/EHlVGIZ/L2nNuKDh23LPVFW7PhiUKw1CcyZJNNe7lj
RGjSpxZlfZ3tb48jozLVVdAa21dIVvStRtD/DxFax9kwLPEPqqByzX+Zj2ZwvHiW
vSj2MGVICt3GjOr3qLWyt/sXuigJrtpyAp8s0HkDw6Q63DoxET4gaAjB5wIDAQAB
o1YwVDAdBgNVHQ4EFgQUGeobsybBXJ2f8gky/sFEomxhOwcwHwYDVR0jBBgwFoAU
GeobsybBXJ2f8gky/sFEomxhOwcwEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG
9w0BAQsFAAOCAQEAaza2m8obVWOaTGWUuG4wBEXhM7Fm7ReNq56YUrj+HIHr1mwQ
n1x2jADl7vhwIIzMqxEUknvnfaJfR0zKGlCy0HxiHsHqKAJWrmVnyAdc6Oftp8ox
YS+yDVSOsURziDjmfDS0fH5O4Sy0DQz8cPJtyCsBWcTr5SQLgDTj+IdcN6bI/gxy
7x+KzDtiU+X7CiceF9USYVMv1yTxYvutdW/obZRfwyQr5KqUPiCXFo1tpA/kgdvw
YGu0LuSK5ECoTwdXr0bUQ+NFY2PMR3ipi9SnPnuC5BhebzGfgTa/bXjZPtZ6gu2e
2hejksHNRy8ZEUQP3pA5S8y+HN25HMeZfLqQHA==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCet+0pjwAlFNAe
Z3nyRTA7mpHlPqskxUoop4I1D83XwGp6TP0/OCVibaphg1b7AYsggVUC64uW0ZLK
KFOEEvJ3jCFeOWApicndOsvBL96XuNjAMCwwFbUiegcODC6vEO5p83xgjMo0G0Cr
06C6IYKL/VJhNrSJsglc4eGu3FPVBV/uQe2yd1CnrjWWMOnWjaNDQ0kAOfXMcwJa
cImUoy5pcIPgrkdHpb5u/DHf2a4DpgwHk7Cdn1PQz5qZoy1zSG5ctGF1FXN4KJJC
ITdUUZJMnrx95AaHXqiopAA+tCi9dxxHn3vwO9qS7NQPeFqk+XnwrPrXMo1em6/q
aFEKygnxAgMBAAECggEAAfW49LqqkIqngDycXaC7lfzUYN2LfPZ/si1RVwjlLI0Y
c36oYOO3WHYMtMwLoqlT97iJHXJdpKTdWGgzkc2vF0lQbyqrtI+aQcRUWEQYTkzO
xl7B6O0Xsb+Kf0Kov+ooW4ARcNrEM20DuStxkGLSCxr7ba5gf9PGOFKkuAVJ9rcn
iGFB7QlftpA9a0wpEr0rDWxrYSdsF8VsWeg/qnXyO7njKRk9OxJAATknuYUwCM/8
yELyAA8qiFmi29ywC7ZzDy2b/xmmraiZVTqHpokMAMDXPoxst7vLAddMWNCz2n7V
PYi5oFW6UXAbb+eXXABnnD09s3EidbjGalAtcJDCkQKBgQDZOLnoTXhAo2yaDZEi
BA5B4nB/KMf/TpfjbvTxkyYamwOzVNxGbuf8R7qZOCiP09mEdWN4PPvgPQYtqN4I
1xGzbIWrz2N+tB6e/ljHTfGH1pVQaBZR+kH7yUgm0Jzz97FB/bCE26o6cA1JObVR
Z2tjGAY3xQp5yTU4A3AnefvWyQKBgQC7DYp0fuuvyhBr/c3FKkkJFvdjZwxWwbTN
/WdTIpcI/wZGyJb6+aWc/dgIlbAmzpiKVuGa/upIeHLrPaBluqnK88RdAU8C0vo3
BD3WvIafXQQfzXdCdbMFFvA7xs8bFhCX0eR5obQjNSpllPtXMIBfPj6Lz7KWHsbU
kAZlmASl6QKBgQClEXBk0YoSRJ4gqzKg0Mgs1PB8EPd6UbUmhYjpktKf4TB8tH33
oAv2MGPiT0Szl19yQupl0qHtEzKAvBSOTzEKUet6VkzbWfDzDYFVYyup+CzbP13G
yhAeUCTeKiU7V/yBk1DyhSLk6YWPIaVhyN9YHEeNVdTSG06v1B0EQrRX0QKBgDD5
d6HOxViuWCRePGz+CEdX+wfMqD9i0jsIuO/cY50YYIVR6a3mhUUYdWKyBn/yoZ++
5azWR3MYDkanSQK5n9Eih+wt9ojvwlF7S9GYZMr+6KCHBE2jUW7otZeBEyKtYUTO
0LpD6004k1QrcK9AZKwLOQcUJ1cE0dw/pV0eM0FZAoGBAIYx8pyZIh3C76F1/46q
r4lQJouYGog6Ipz8GnOoNUgqmm7Z8iKyfIlkPUbZPlrV90aJWciWEfaypY6a1rli
k7T9zRJrRRkqByoP7KhHf6IEJQKdxe2GzMxft0w2+BCCLfog0l7hf1Su8kw+TeUo
23cOUymQgvmja7fMZUYR7eLm
-----END PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFNTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQKS4572xxWK5/vKx1
OHBHGAICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEEHpcs1cB3kAvuCb1
fOrBlswEggTQpVy7MEEJfJwoyZOqBc5suItx/IRHEmsdJlchEn9tNwpEZYExzZPn
sdezShXut6xyJLxxgUMy9WMN4n+ir852j2ERYvT3tXOO6pXHIzaLlpcOkba6MSIN
QwkNBVZrvGv/njzQNDf+7h2+MCShAnap8iV3R8q/n/fIt0eM0fj+KIrgE+VDXSDe
yHcQHbSNFKQFq9+kUY14ZNWqPS/1UW3L0oJongmIv7o1nnBaVheZ75yTQgnqR18D
LXaRw/fI3YlkrMZw5094PPcUbUUi5lkvZalOZedhRYHtZz4AKrXHTdO1cRwX2fzz
iVyK13KU6+ZHZUYrFuicaWB0UGlJ+xCgRj0zlhLFTQoviGQbv44ywGt6CUBJnX4P
s0OZ2YLMyY2d4LfpYksXw+yJuB/Rp03VhiNr2niaiNgLOBJtNcA1QHEDoh0JyP2I
sZf/Fgb9/zrZp3R0mOZIWPh989tuTMdDRO64vf1yeOts6sYAgHG+k9q7XztzL+yJ
RAnEsjlq+7OmXScqq3sR9FJ5WlFvSMnloZAUOegiFU7hUpVSoZZiqKM3RPx7SHqT
8oXava1hn50YNUY1KS1dAA/pQv/Bnk7/yB1hLlSiTLoKNvDsPzS7PKIYbxPzb0Yp
VB8hZbxp7a45wKLnLXJ5BnoAncn/88ZGfzH/VBRC+v8r+wbTeRJg5nhzXGd2iiav
k4aJ4dIRxkrBrKfoP1XaOidGnpRChPd+jgGDaDY8fXJCmIIjbgfyiGBDna561itx
zBDnyVf8n/tIU3d23eJHBgDKpE7B8jzMk1aAwY5YRnA1Wh5q2UVrX7xZk8KN0hPW
Q71QZhmLIUCYs/01PToJLgXpO51qpyPafy+p/KOiYpQlXZy1pZIG/jJeCIFKxwWv
zPq02GW8m+5S73Q8mV0sAUJQpg+9G2I9yGo5HiLuKu/VqSpvuTa57yidkjZTDiqL
Pu6TgvymUhd5eusu136cYIDiGDilae1P8zd9i1F0gSyiVxQQm+u3NrAswSDfvRCs
VQtgl623KmQSbFd5kwuj4YPGbf1CgrfHhkYdsgHR7uxbghO01YqhyRNXQn0sruye
lNnSxTH3opfd1RyzxX2Jhz8okYcxUksetmXxT5gio6RL/dxfy32I1F0CDR2SQGDl
90raxg6LhuqVLHUJ1+pDDKTHuuBYY92ZLhW8STN/dfJOKMw28xVgpisk5+HJc9Gu
8uc7eVXVTO4q2fsr7huX2qfJVgOkUeSgjKiGM9jTZZqFCizqYMEC7bmmNYgBz+IL
rA4w1l6fXQZo3TbM9k33BUe5NUQIDUMKYyUAqtWTpcaw9o0Xu85japwMSH9fEOQt
aTrWS/9ywK36cZr1+fchEUGsWvsCC9Dr0N6eKoF+t3y09bh7dPksS10Qs3MxdKjx
8t8EGKqFCJxcyCrtQndOvhXVOyYdclcV5ogcAx+Y93o/Kk1aqKCAVs1Ogs50gpNX
IN8yBoUe+tRKp+YyxSYOuTpjAbyB7Ce7d7/8mrD51/SgvitLNlpuBSrATGCyZz0H
TreJYbyr/jjCVWQBzH/jLC/GHuwlwHvK/jnS8qDLEbAsQeDG6sz9j7dRVd29yzpU
poqYCzCfiPS8TyEcuoaoSRyMsYJzYSffJs0zAbZJEOEya92jo2mEAwM=
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIEPTCCAyWgAwIBAgIUUguftTibAj9QRaJvfVHkPKmldKEwDQYJKoZIhvcNAQEL
BQAwejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkcnNhMjA0
OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzE1MzExN1oX
DTM0MDMwNTE1MzExN1owcjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTAL
BgNVBAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTElMCMG
A1UEAwwccnNhMjA0OC1lbmMtcGtleS1jZXJ0aWZpY2F0ZTCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBAJ637SmPACUU0B5nefJFMDuakeU+qyTFSiingjUP
zdfAanpM/T84JWJtqmGDVvsBiyCBVQLri5bRksooU4QS8neMIV45YCmJyd06y8Ev
3pe42MAwLDAVtSJ6Bw4MLq8Q7mnzfGCMyjQbQKvToLohgov9UmE2tImyCVzh4a7c
U9UFX+5B7bJ3UKeuNZYw6daNo0NDSQA59cxzAlpwiZSjLmlwg+CuR0elvm78Md/Z
rgOmDAeTsJ2fU9DPmpmjLXNIbly0YXUVc3gokkIhN1RRkkyevH3kBodeqKikAD60
KL13HEefe/A72pLs1A94WqT5efCs+tcyjV6br+poUQrKCfECAwEAAaOBwjCBvzB9
BgNVHREEdjB0giEqLmlwdjRfaXB2Nl9hZGRyZXNzZXMub25sb2NhbGhvc3SCH2lw
djRfaXB2Nl9hZGRyZXNzZXMub25sb2NhbGhvc3SHBH8KFDKHBH8KFDyHBH8KFEaH
BH8KFFCHBH8KFFqHEAAAAAAAAAAAAAAAAAAAAAEwHQYDVR0OBBYEFCjmfaa8abxB
H3ItrMnN2Sf0B7r4MB8GA1UdIwQYMBaAFBnqG7MmwVydn/IJMv7BRKJsYTsHMA0G
CSqGSIb3DQEBCwUAA4IBAQCoLf2jZQ+LpUiZMJe1M/50Bt9YKxKoyCeCvNuAvYPr
esd01d/wi9V4SUtJASQXsCaM0Yq1kcdJie4rp57goPgotcQ28xQHDBxML1GnhXGQ
EMJXwwCK9K6qsuzYlSTaZSq/lVhLuz0Qe42YXjRQ8LwOu6ox/vn0SBlGblwaxI3w
KW0oqetk8jCSFAd+7qBbm9Oq1ncI0Ol+5uRVpTVaIla9nhXfyHjBSJeD4cTJyFgQ
65x2wk0OuO81AVFDdCj+QpVfDNRegpH+Fr1eRWUrJ/xxbwT0DkO/rRPKpcBV/jIG
AzANRvh13xVKjQYbEdCcz5yDDWMMBMY5Cx1Wojl/MX/1
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo rsa2048Chain_EncryptedPrivateKey()
{
/*
 As RSA 2048 with intermediate certificate, but encrypting private key with password password,
 using the following command: openssl pkcs8 -topk8 -in rsa2048chain.key -out rsa2048chain-encrypted.key

 RSA 2048 with intermediate certificate
 ========================================================
 Root CA key: openssl genrsa -out ca.key 2048
 CA Certificate: openssl req -x509 -new -key ca.key -sha256 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Intermediate Certificate key: openssl genrsa -out intermediate-cert.key 2048
 Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-intermediate-certificate"
 Sign Intermediate Certificate: openssl x509 -req -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -extfile <(printf "basicConstraints=critical,CA:TRUE,pathlen:10") -days 3000 -sha256
 Certificate key: openssl genrsa -out leaf-cert.key 2048
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-leaf-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in leaf-cert.key -out leaf-cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID0jCCArqgAwIBAgIUORaj/S1hgN2JgCfO/PNy1nKq8yIwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzIwMDQyN1oXDTM1
MTAyNjIwMDQyN1owdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UE
AwwhcnNhMjA0OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAweewMez+ynhWLsIDAxbW2PWRNQRwS6MUUzc/
N7Ts7teDvuZY7lSwyEKhJi157Y0YBxKeg7TBmWEOysWYoHobceGkID8VT/jOMfu8
TJXK21UDeWwQVlsAh8Z8xdl5kcJK4hY4C6fY0bPo1ZKz+fF+vqqEix/DWB81U9C0
Qya/dNYzo4XUcUgNeyuUp5u12Hm1jmGQ/0AUNZ363PfO8fHPbNjQYYMhD4OAru1P
FcIOHk9EEEVFm+NLHB91Q5ktJx8DKZLhC1krbIyKnFdhgjiQlYRkzm6ZQQ3aOD7N
BGPMuq9sRIWf/8AUNwqTVxxMUWJG0zWtz12wkQ3Nnz29BIshyQIDAQABo1YwVDAd
BgNVHQ4EFgQU4mieJ1fK0wmk+4xDtfP+C5qdm8MwHwYDVR0jBBgwFoAU4mieJ1fK
0wmk+4xDtfP+C5qdm8MwEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG9w0BAQsF
AAOCAQEApu06TGT+Z08/mdANOl5v4blhQgMXAeg3czvdPWL3eqNeIHGdZj2zX6ZP
Z3Z58fQMsGOUs30e2yCgvspRoCy01JtSh0+ThF1uPrQAIVWTcbopz2EDMxVIb4GW
muuHoVLQzjrX5swqZ7B91eMw9WL70vjcYf63sS1Wy1jwQZYsGkp1cLHCB8ulRuog
WmRPsCMbHkbpNTBJtWAf2bXCxEHm9MS7h9UOPAJNsGK6+kXQHvx3XJ7a2afgwfi4
J5lVOLweEZ7aszhVqP31m0Lso467BwnM8YwLCTC1my57dhhsuZIDIVdY4Vd/IQAE
3CpwfTxyVaxXNvhQuczqyqAKAoTc9g==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDdb4T39Wyt0mp3
OxVvAVkyBE7hHVc9oO0wB6C49i6Rly5QV60HsfrE00AVwHe0egANMVUDW3zoiQgk
iib/cWnESWQYCkgJi0bMRH1qYGjZ218nu2l+PqpdL1MZmtA94tSeEX94JFDwGGpm
8ioUUqW9lfD1IteleoVXX+BJ64rORps4ZLlwmyDaw/Fda8ZKRGz6dfJUIA122wGe
nqwVZLt9Q7IyC+tGDM7K/kxCNzjnP8peEq0EBlXPNHWHtQpKFImoy4Oc5n0/pmno
Fj9ovaxJ/htvEfAzvviEFc7Lj/0By6a6EwE82B1hN0KHQnlQsAC2qN0rKut9I3yE
7eiWhA+bAgMBAAECggEAJwSrjXW1fLuC/xduI209Zk7UB17ALJ/aq4fsjiry8byN
8KJCXwTBh0UiGbMcZLTKpYh3zmukiuT6hlXBoWk+ldPIy2OkZeogw4WeA8yRLTI2
hi3D3Pb11g2suupIn18E0AWNTII5sNTcit9JAuO9SLbPCqLlFMeOD2NZhiz2/Hmo
k+ry2RWMQhUzGw/BrsLcOHS7K3hbexOoGvDaYBhJ4QJe2Gdc5u45vunvQtJvfLab
KSjumtOsFltYrGjnUgfHs7xTYOUyCCKu3ujIzjOIqfd+eXTIPJi91Ahg3eqVl3iw
5ggNhQ7vgrIQXVjm0Q3knCOjUeymE5akfc+P68pDQQKBgQD2L36/18LlxHn4yn6c
B7VR84cYmJduPkD7TYAADiP/BH4TvmmyCShare4qA/UDeBk29oo3iPpzKutWyCpn
jK58yRbebkB5eH37kF2cKSmwmmPVtn+vbAzZmVnUWpviy3PEjaiTbxo4CnXu/KHg
Fg3ZuzSk6ilg0eogcfPiAaSYmQKBgQDmQ27p/WQ3XvgCrUmiiLAGpbnVB0VuSTPk
Hrtq1RfnGEowWLd56yenKgQ0DL3EqRi+3Aw7lRDgBnWn17bjwtPg0F1SVZA5krxy
6xqgyblASS5vjcIwFDL/gmfOrKSSwzHVnDY1037WI7usDPL48/Hr4XeZ7/WSHLXE
QVoFRAsGUwKBgCbPvg/ImDVZEFGkuukmXfOZjQbXp6f9sIjt4uwfTpV5w9ahAlDL
GM2S9iUxkoMFgBQmMgwnUDSv44I+9FB24VtS6IQQpaGA2Xu7esaPxr7M2EhtCRxC
0JPNlVfhIQpUQOAP5S+5KE0FeSpAMdT+JRjaZqZJNFTLQznyPRy4XZahAoGAHqmG
IISMjs8HqZuXvZwEDK6O4243casvzMXG4UAFEdHrNyrpK8QDoL/4lVHJB2IZGHlG
67w65goWKsaP5R5xhnNYqfkY3oHKtGbYQeb60zOrilFPNYXLELk1x5C3zY9OmIpX
sa07K5rQh+WliT86zNVWTtpkUhOepMarDR51dV8CgYEAqAdMSm7z6USWE7hKKB1O
9hKuDH/o7TL1MjR4CjU4HQFrQBN4L22ptFSkiHLpsTMaSWW1xxGkl8c+e2WRBDyL
8izP5LUjVYt/MX5odX5k72qPG7fl9XRJSZBDJCq3ICX205BnEwaCmJGR+DBDbvDa
hXX+7BgIQGgLagM95pFIhDQ=
-----END PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFNTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQCX41ImLw7vnomA5m
DVlIiAICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEEIA7IkIVRJ52rgNV
oGPhdEcEggTQtE6jClw9swuEm95JF/ipma2WLPrVmS3xVDM+r4lFN3ziRVnhO7VL
B7VINcZiByYl2N60HYC+7dkXhi84n4bFW9YZLM3xtYXQFoQ1ePWeb2+/+6VnjkK8
dUq4Wbmdk50MI2IaQyStCV+Y7cp+2X8ZV+nFwfhOFL26Ar/HOxr6VvWpGMBbI5pw
9sp+0yg/AZTQ5V4gb0MkEVHb3Kgb/6XoXARwVKhBbJUF7hmLtBjmtSvs9UwKoxu5
0bAZedKcqEUg5BN1Ler2MLTKE81z+Q6Ldp99q1ScsoshrpvcJkFAKGCe6yxaEBuE
gzqmTZggq6CEbsaQ/IuD3DA6hpcCkcsFsll76hxgsGinWm43QZ/IvZOHuC6QvRkR
pF2oihHgo2fPjzA0hEEopsHAzd6oVcYiIFAYilQjB+DmIfACdRJSikAmovk8d+E6
vFfaa8l9pNfNGeWfe/Cs3wzEJFW8vlpo7ySL6Fs77Ud3q1XuNb64A/lL7ScUZ0Wl
cV/A2nZU0284d8V6ln9rTfo+BMLNJvVzd3CGRm8V7DANbE09IvKlLe8tI9X3F3ih
100whoDmODTRUvyruUD+aWRmH8VsuVSob0nKkL5ztqmTa1Ib0TqVDuQueI9+ZmU9
UtHFJnJfVBe78jOZzAeXwMFgtwKCNFxMXnhiQJxMTNCyZK7aTYiQHfuM+RA1BMTD
Vyn9JG4XgsfNuoS158riYPi8LIszejyxECI5g5EI6SEcOOq1h5qZmtM4eylcvA6u
C9DrKw3xSJ7oUvHhxgeDWlMKMOxK41Czh4X9X3X284aajqn7RVjp1vTzlkCPh0oz
sWEcyYIsTcR1r+9uJcty+x3sy+z7OPHDp/m7zPm0r2txM2fweZlszWXER6E4TjYi
H1MEqPSIcHQqJuC1Dm8dIrQhpvY5kQPuwpz3XwOzj27akGIfzwlUpFUsyTdAIAmz
d3F1E2P6Rd3cAKXj4jYfyykMxPfp+V5I9bgjtO12Hjj2T9mkdpf335YyRpMs5tPh
KWQpi8/qbvxXEIkPhNzy38M87pcebSrlYIUbrV/h4txpQRVfe35AvWV22E5wWMDU
ajSYppwrEmYxpN6tXm0pWso+g3y91fkECvMB/Mzfv2acFBA466nafFhC6QJOXN2O
4MaVKiCie6tPtt0VLwURmNwqKSwNvVzGYcy66ZIOZTIHfGbRFBFSPoDb1I+pL/oi
97ZnYo1CTyI2shxE/ysovvSgS4NkOMSEtprZsvllFiUWk2nYfRiGHloZIeEkbPdV
bbp/jD6c44MzZ/VlDAZkSkE6FIsfkSMPiqfzc/RFWL+xz/Z6D1Gzq/qOqtZO9fFE
cPfSSTV3mJ4BvrUrAOqocphufMbDrzBzViVblHklAIVQnqQsGlq+FENTV35F78no
4DiOCLjqrqfSkiSm9tBqBgWKp+zxlT4hWLxhTwfidZg1kj0Lk3rl+mA8qGgTKyYC
P78GI8Qt+WJXruxXB/09rAbYmmFlDrlB6xpPIGUNyZi3Tw2kRad8qvQNt+4YKfsk
Vj8n2/DvSZ0t9vjFvchgmJDeJUSW6YyjhZiu5SJb8EMgk5ozc3AUKMo2uXDrL0yX
wH0SMtXrC/5C3oF3bFBurQ81eh2y2Maf4tOROtIkRIwep1fy7PmkcpY=
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIEQTCCAymgAwIBAgIUZYOhklqcUCeTdBAIDyMLjSy7lrswDQYJKoZIhvcNAQEL
BQAwfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UEAwwmcnNhMjA0
OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwHhcNMjUxMjE3MjAwNTIw
WhcNMzQwMzA1MjAwNTIwWjB0MQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTEN
MAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMScw
JQYDVQQDDB5yc2EyMDQ4LWNoYWluLWxlYWYtY2VydGlmaWNhdGUwggEiMA0GCSqG
SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDdb4T39Wyt0mp3OxVvAVkyBE7hHVc9oO0w
B6C49i6Rly5QV60HsfrE00AVwHe0egANMVUDW3zoiQgkiib/cWnESWQYCkgJi0bM
RH1qYGjZ218nu2l+PqpdL1MZmtA94tSeEX94JFDwGGpm8ioUUqW9lfD1IteleoVX
X+BJ64rORps4ZLlwmyDaw/Fda8ZKRGz6dfJUIA122wGenqwVZLt9Q7IyC+tGDM7K
/kxCNzjnP8peEq0EBlXPNHWHtQpKFImoy4Oc5n0/pmnoFj9ovaxJ/htvEfAzvviE
Fc7Lj/0By6a6EwE82B1hN0KHQnlQsAC2qN0rKut9I3yE7eiWhA+bAgMBAAGjgcIw
gb8wfQYDVR0RBHYwdIIhKi5pcHY0X2lwdjZfYWRkcmVzc2VzLm9ubG9jYWxob3N0
gh9pcHY0X2lwdjZfYWRkcmVzc2VzLm9ubG9jYWxob3N0hwR/ChQyhwR/ChQ8hwR/
ChRGhwR/ChRQhwR/ChRahxAAAAAAAAAAAAAAAAAAAAABMB0GA1UdDgQWBBSJAwit
rpvzPPSjy2hX+dVJS9h6mTAfBgNVHSMEGDAWgBR57515zOdUWfdhZoVJw0fzlBiP
hzANBgkqhkiG9w0BAQsFAAOCAQEABXRifNwtzvwCVqqSquOg3Y8JBMKw8tvhtYQs
X3OAL0X4c3yyXLsWbQx9cpQQrRGwf+xZQ6RhCxY3tNMu2poX36NP91o9XLlYIIfp
KkPCGhEnf0/WkPcPdQ5c/x2o6RQbbs0kmHzwWUI/HRJMgZnY8C2x+X9DCUKF+WCG
x7fa9yTiNulmQcx7KFl1Sh309Ppi2z8G6Eq6C5dGzvQohs6obA76TXYKrFJ6Zqwl
tY7hUcaIxBzf+cu5Z+wZZBzofsbnPK5sEHQhPKYxTelJnSYkuFH9r/T5DxtCkt2h
M6r0n6ynR2d+khY0oTigbztf1kMJ3Ifqzygm5W1xunthXSVlKQ==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIID1zCCAr+gAwIBAgIUBshsKhleJIysWSHi8NIwegjoJ2AwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzIwMDQ1NVoXDTM0
MDMwNTIwMDQ1NVowfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UE
AwwmcnNhMjA0OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwggEiMA0G
CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDyLyoE52ELczWdst1IEjJalHVYcPWm
K+Cz75uQCRvxnUEroMOBPDsZmf09lfETZDph2iEBecJy+8PTaHWNMGA5HTmmKLeH
tD/sUjbh9+hAOub1tkI1rU/1iJ00RHLoVX52OypP3ZRunl59WvG6DwdrHQw9Dpuu
qvmdGgBczkmKJOS8bPxJ8iNUJJ3rc+SJHWRoDlueQYwmdCua3kGHRVYfX7ToNOje
OLtuDxT7BX2CCqYDegv1yKCjJGDWI8dVJUIbqmhfBeWZesj1GeRiifQOQwmP0o7N
+XZorvO6F5lymEuMfkkl0BltAvspPVk8smBzl3YCq/62oi1DOTiMx8Q/AgMBAAGj
VjBUMBIGA1UdEwEB/wQIMAYBAf8CAQowHQYDVR0OBBYEFHnvnXnM51RZ92FmhUnD
R/OUGI+HMB8GA1UdIwQYMBaAFOJonidXytMJpPuMQ7Xz/guanZvDMA0GCSqGSIb3
DQEBCwUAA4IBAQCGWe8NR7M17YBSb3HIJDUANFA6DzN73/SoBgNAsq10CnN6fG9P
+j9EzaF5YzHrrZbnaKr4NKDtMg17P4Azen6EqEU5K40KxzTG2pNlXgONRupre2rE
OBliawT4a9nrkVe2/hADc9uPrhIiB71QHfMZqQBFH8qGTZUjUBnQsHuGOmQE4lE3
G0InVZjsADlNWPpYPBEZOqWjMlVZR0G9GSaqVCc3om7nABsybMcYmRllBmWaZB78
sVBreqx9UHdBRiY1j/aNIFhnxeksbdAAhYLLMVfDaCls3SHbaaVY3MKvnqwlxcxs
dpgQnfo2YfZ7rDS1nZWYaKsoLBZmFtz2P8nE
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

/*
static TlsTestCertificateInfo dsa2048()
{
 // DSA 2048 OpenSSL Commands
 // ========================================================
 // DSA Key Parameters: openssl dsaparam -out dsa-params.pem 2048
 // Root CA key: openssl gendsa -out ca.key dsa-params.pem
 // CA Certificate: openssl req -x509 -new -key ca.key -sha256 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ca.root-certificate"
 // Certificate key: openssl gendsa -out cert.key dsa-params.pem
 // Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=certificate"
 // Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha256

    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIIFDTCCBLqgAwIBAgIUXW2Q2dW4rG2ki7JR3zigBbuxrKAwCwYJYIZIAWUDBAMC
MGkxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxHDAaBgNVBAMME2NhLnJvb3Qt
Y2VydGlmaWNhdGUwHhcNMjQwMTE3MTUzNzAyWhcNMzMxMTI1MTUzNzAyWjBpMQsw
CQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwEQ2l0eTEOMAwGA1UE
CgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMRwwGgYDVQQDDBNjYS5yb290LWNlcnRp
ZmljYXRlMIIDQzCCAjUGByqGSM44BAEwggIoAoIBAQDrgwFF54yQFkj6l11MpBOn
FuL4OfcS6Wz1kIgkICKe/fIznpaKxOS+1z2kY4YBum76D7VmE9yJjX0UuATX8TKv
ijawGL37YpCK1rovLdXINT5+ddoOjslJmq1ZBQhe+79F/Wbw2YBkwnD3jfaF7I6F
hzIYSbK4499fnd1EiD1QTKEtVwNv+v7CDwFXJVWAbfkxU6L+/ykIkU3ee47hWNLz
9rH5YP0pCWzUsRiA4W3bxUROcXmv0aLkpNj3UgS1rIUD/RKiqFHAeN3vfEM7ulbA
CJvsfbepEa1qTCwCXm2hU26tN4NZET1MImajARI9nIMnhjMuh4bDL/rEiolSRN2b
Ah0AvlKqytfbM6nXEdmTLJ7/KoiwtqR3hW+LMkOdawKCAQAHJ76pmxFcGsP2t5ZO
NYObn0BJk4qmc8zNCgapeVAMLnu4RQX48yH4IGMWM57w64aSUf8b+XiGrsUt1emJ
/3tqVQgGme/Cajb9rljWNARrPKhOWk9IHJhBNoFSV0wviaPfiALLulJCGVo5YkIK
8PVElnJob+IirS86UTy7CyB5gYZqBG8rn/Z5Md4bsfa3LjlvcocyGc2WD479jSdA
4qljwgYQ2upSS3FHoJ3uR1sVcqY8+t6U8IMeYK+jfet7sZTEzFwiqLxJMOMbrUVO
TRMDJiNfacPZQlajgLriN08nJZI6WZ6CG9aql7Peji9R6Ya/RbZr9zdDvZStRTL2
vowdA4IBBgACggEBAOTF6qZNDQjcz1Flcpr0M8Waa9lvuVj7hg5C5y5MLiN4/4aw
LTN2M/pbJjxXe36mmPBvjZgWliIGnL9MSagbGIhT4SF/BvPzZcfGg04M9uUuUDd/
p2cnNcrDw43KFJwEgbbcpjuS1XknT4Ti8yreJNwUxMc0AnTRaKFBil9xsO/2PGj/
8Ck0rpN9TnWYwYXBRcxXweg4VghyFe2mZblEBJ+unKHWQtdnNqGPJkRuyiADKQir
ZF4UYx7O4qBZqcuEhwJQK3qU/G+u2zIpTmAGSSpsUVw6bZhz26jHLa9xi+LE7RZI
/aaefKhPzYNPmBhUF9ZW+prtFYLjIWaPcOEEPeajUzBRMB0GA1UdDgQWBBQHC6D7
FdN/YaFubsV68rvwoK1ILzAfBgNVHSMEGDAWgBQHC6D7FdN/YaFubsV68rvwoK1I
LzAPBgNVHRMBAf8EBTADAQH/MAsGCWCGSAFlAwQDAgNAADA9Ahxy+3QcXEDzfYnM
gKm7vbxmsakXUXo5DWnYDS6eAh0Ak2gFE7ARxS9pKFl8EQKIoAmi7ooZFX5AGi2Y
6Q==
-----END CERTIFICATE-----)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIICXQIBADCCAjUGByqGSM44BAEwggIoAoIBAQDrgwFF54yQFkj6l11MpBOnFuL4
OfcS6Wz1kIgkICKe/fIznpaKxOS+1z2kY4YBum76D7VmE9yJjX0UuATX8TKvijaw
GL37YpCK1rovLdXINT5+ddoOjslJmq1ZBQhe+79F/Wbw2YBkwnD3jfaF7I6FhzIY
SbK4499fnd1EiD1QTKEtVwNv+v7CDwFXJVWAbfkxU6L+/ykIkU3ee47hWNLz9rH5
YP0pCWzUsRiA4W3bxUROcXmv0aLkpNj3UgS1rIUD/RKiqFHAeN3vfEM7ulbACJvs
fbepEa1qTCwCXm2hU26tN4NZET1MImajARI9nIMnhjMuh4bDL/rEiolSRN2bAh0A
vlKqytfbM6nXEdmTLJ7/KoiwtqR3hW+LMkOdawKCAQAHJ76pmxFcGsP2t5ZONYOb
n0BJk4qmc8zNCgapeVAMLnu4RQX48yH4IGMWM57w64aSUf8b+XiGrsUt1emJ/3tq
VQgGme/Cajb9rljWNARrPKhOWk9IHJhBNoFSV0wviaPfiALLulJCGVo5YkIK8PVE
lnJob+IirS86UTy7CyB5gYZqBG8rn/Z5Md4bsfa3LjlvcocyGc2WD479jSdA4qlj
wgYQ2upSS3FHoJ3uR1sVcqY8+t6U8IMeYK+jfet7sZTEzFwiqLxJMOMbrUVOTRMD
JiNfacPZQlajgLriN08nJZI6WZ6CG9aql7Peji9R6Ya/RbZr9zdDvZStRTL2vowd
BB8CHQC6/ISmdVd7qCtuD2cb1kzVe94Kk8dDrwOUWzxR
-----END PRIVATE KEY-----)";
    testCertificateInfo.certificate = R"(-----BEGIN CERTIFICATE-----
MIIFGTCCBMegAwIBAgIUVJZPldhpa+H99JepKpDTTDzcVzowCwYJYIZIAWUDBAMC
MGkxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxHDAaBgNVBAMME2NhLnJvb3Qt
Y2VydGlmaWNhdGUwHhcNMjQwMTE3MTUzNzMxWhcNMzIwNDA0MTUzNzMxWjBhMQsw
CQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwEQ2l0eTEOMAwGA1UE
CgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMRQwEgYDVQQDDAtjZXJ0aWZpY2F0ZTCC
A0IwggI1BgcqhkjOOAQBMIICKAKCAQEA64MBReeMkBZI+pddTKQTpxbi+Dn3Euls
9ZCIJCAinv3yM56WisTkvtc9pGOGAbpu+g+1ZhPciY19FLgE1/Eyr4o2sBi9+2KQ
ita6Ly3VyDU+fnXaDo7JSZqtWQUIXvu/Rf1m8NmAZMJw9432heyOhYcyGEmyuOPf
X53dRIg9UEyhLVcDb/r+wg8BVyVVgG35MVOi/v8pCJFN3nuO4VjS8/ax+WD9KQls
1LEYgOFt28VETnF5r9Gi5KTY91IEtayFA/0SoqhRwHjd73xDO7pWwAib7H23qRGt
akwsAl5toVNurTeDWRE9TCJmowESPZyDJ4YzLoeGwy/6xIqJUkTdmwIdAL5SqsrX
2zOp1xHZkyye/yqIsLakd4VvizJDnWsCggEABye+qZsRXBrD9reWTjWDm59ASZOK
pnPMzQoGqXlQDC57uEUF+PMh+CBjFjOe8OuGklH/G/l4hq7FLdXpif97alUIBpnv
wmo2/a5Y1jQEazyoTlpPSByYQTaBUldML4mj34gCy7pSQhlaOWJCCvD1RJZyaG/i
Iq0vOlE8uwsgeYGGagRvK5/2eTHeG7H2ty45b3KHMhnNlg+O/Y0nQOKpY8IGENrq
UktxR6Cd7kdbFXKmPPrelPCDHmCvo33re7GUxMxcIqi8STDjG61FTk0TAyYjX2nD
2UJWo4C64jdPJyWSOlmeghvWqpez3o4vUemGv0W2a/c3Q72UrUUy9r6MHQOCAQUA
AoIBAGbXCNP1Aeitg5w9UgH2VT1r/deba1cOT6cEdmHxsvXbhSoKazb/q9dDQfgA
auY56FL5Jxa60UNZv3oDj8B4PPPLVIv6A9sL2LtUG9tB6+pyme45r4o9eAGmaX7H
FJ+a4mb8nj7IDa/tBzYzkJFBUm7LTgymwQxA3GSh4dOkyaL0dL0K8hZhk/BPmN5O
seqsL0jE5/Wn0cWkg68uRhe0jIip7K3oeE8BMbpPPgTgvR8Y1roqQtQYCKO2nX5Z
vVRsSCvzoY4PJfmeCD7OKAtLfL2/IS4rbpkFBRAk07MXrGb11Z1xLuiOwuUMHsdP
Q4uv8fvhcFFct1iljZeF1JnsoD+jaTBnMCUGA1UdEQQeMByCDSouZXhhbXBsZS5j
b22CC2V4YW1wbGUuY29tMB0GA1UdDgQWBBTi5ZUiUI50Xu/JKPZ6E2mzBtfNhTAf
BgNVHSMEGDAWgBQHC6D7FdN/YaFubsV68rvwoK1ILzALBglghkgBZQMEAwIDPwAw
PAIcUEKZCCrE/F6vH0AbmAT5BtGUvys/SKeCvYLPVAIcY0eBPLBNhE4cYrAWpjlX
wDqkvaNHVVcd6D0ZLw==
-----END CERTIFICATE-----)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo dsa2048Chain()
{

 // DSA 2048 With Intermediate Certificate
 // ========================================================
 // DSA Key Parameters: openssl dsaparam -out dsa-params.pem 2048
 // Root CA key: openssl gendsa -out ca.key dsa-params.pem
 // CA Certificate: openssl req -x509 -new -key ca.key -sha256 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ca.root-certificate"
 // Intermediate Certificate key: openssl gendsa -out intermediate-cert.key dsa-params.pem
 // Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=intermediate-certificate" -addext basicConstraints=CA:TRUE
 // Sign Intermediate Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -days 3000 -sha256
 // Certificate key: openssl gendsa -out leaf-cert.key dsa-params.pem
 // Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=leaf-certificate"
 // Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256

    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIIFDTCCBLmgAwIBAgIUWjWq/T2zTWYXCuY1uwW0/pdHvkQwCwYJYIZIAWUDBAMC
MGkxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxHDAaBgNVBAMME2NhLnJvb3Qt
Y2VydGlmaWNhdGUwHhcNMjQwMTE3MTU0MTMyWhcNMzMxMTI1MTU0MTMyWjBpMQsw
CQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwEQ2l0eTEOMAwGA1UE
CgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMRwwGgYDVQQDDBNjYS5yb290LWNlcnRp
ZmljYXRlMIIDQjCCAjUGByqGSM44BAEwggIoAoIBAQCI6TXI5iKCJfsA4x1F+ZgK
nX+0g5jhh8UySd8YnhJ2W8cBMFfKjw7cVJJeXW+hMxGjN07TmMJt9GMg7sNa0QQM
REnxnfd0xXgq6S26lZoKPQJWwmCImBJnw4lTrPBdNHAsngxcrOZ5eJbyFGORSJwV
yaMSLSBoCts7F02IstzZXMSclmIxVps9VzGuZ7AIk+BftM6Js8dPIMo6mlcZBXvL
nIgOe8/VAwBXXH56ttXudOM65QPR+8QyGmYX7UMgXKrO4W7SNhY9QoIQDil/b/GL
wPHmPVv2cdubq3J7DlKOTY/92XU8+K9kHWZ0CF/RxPQZrYbR6pHYAcdaKFjHF4kv
Ah0A+4tlBGVcGzPk6DYCWdfcmZ0156NqZLEu9xfXfQKCAQBGVMMLNBhoizFo8xV/
Z3v7IRcLApKES4gmN9cCqRNIxZMQqblzO9jOm0o+B+ftASks4yrm2VCTOeT9nxks
RkwqEfY/SJPcgDw9WaZTtAlBdtLf8NdASXnTzF6rme1ixJ07nxF9tTzy2AReIr6b
vXMMFn39fLKztawHxPw8Tfys8Kg6CGxozBpyP8zfV7puz/mo2nzGuYXGkZ53q8iG
FpDbm8cT+DRTfmJEshNPj2pGUqDz/Cs9DL19cr9vcGlQBWzIjBRe2HsMFyq3VqX+
63A0c08ZIY95DC987oMXvJmWd7iWHiGFRvGsF12t/auwdD0cv96p0ruEr7fXuZst
mKo/A4IBBQACggEAfY8KM/vlsgXAef333u37xdJHX3Z83y/eocW7AO/ywkds06dF
5qGjixoo+g06b4iRcW2WbUXwrZiWzCGAJVRRo08Pqy682IT7ghv4I9ywYkGDLq28
HVotbR9HwiEw8TG5PiiBDcS4MtmtusZiQENPF1ijCLq3e2H7sYKHGZwgFPU8JySy
ruYpcMTkiO9tT5AqM/Yaqc7X8LM8c5iBzwQeK7EbQDRRdDzsvhiKlxWMlLtI/jV8
BGIoD8pbVJ3yCxSc2RwL69S1RT3F4YOhUDbh8rOQBh+mqX4mnTSVMv2MP0DZ2iOc
XHz5Lb3drg3NIbgUePDVFC++dZxZ9UqX3xV59qNTMFEwHQYDVR0OBBYEFPYVSZ8D
P5CFPIn4KNaaX07USuVYMB8GA1UdIwQYMBaAFPYVSZ8DP5CFPIn4KNaaX07USuVY
MA8GA1UdEwEB/wQFMAMBAf8wCwYJYIZIAWUDBAMCA0EAMD4CHQC+ToMANimRBNPq
5wTduwS6260PfVNE9fUXbUV3Ah0AinKdUCHsXmt2wivzCJM+7CpSSz6MekvpWXhz
qw==
-----END CERTIFICATE-----)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIICXAIBADCCAjUGByqGSM44BAEwggIoAoIBAQCI6TXI5iKCJfsA4x1F+ZgKnX+0
g5jhh8UySd8YnhJ2W8cBMFfKjw7cVJJeXW+hMxGjN07TmMJt9GMg7sNa0QQMREnx
nfd0xXgq6S26lZoKPQJWwmCImBJnw4lTrPBdNHAsngxcrOZ5eJbyFGORSJwVyaMS
LSBoCts7F02IstzZXMSclmIxVps9VzGuZ7AIk+BftM6Js8dPIMo6mlcZBXvLnIgO
e8/VAwBXXH56ttXudOM65QPR+8QyGmYX7UMgXKrO4W7SNhY9QoIQDil/b/GLwPHm
PVv2cdubq3J7DlKOTY/92XU8+K9kHWZ0CF/RxPQZrYbR6pHYAcdaKFjHF4kvAh0A
+4tlBGVcGzPk6DYCWdfcmZ0156NqZLEu9xfXfQKCAQBGVMMLNBhoizFo8xV/Z3v7
IRcLApKES4gmN9cCqRNIxZMQqblzO9jOm0o+B+ftASks4yrm2VCTOeT9nxksRkwq
EfY/SJPcgDw9WaZTtAlBdtLf8NdASXnTzF6rme1ixJ07nxF9tTzy2AReIr6bvXMM
Fn39fLKztawHxPw8Tfys8Kg6CGxozBpyP8zfV7puz/mo2nzGuYXGkZ53q8iGFpDb
m8cT+DRTfmJEshNPj2pGUqDz/Cs9DL19cr9vcGlQBWzIjBRe2HsMFyq3VqX+63A0
c08ZIY95DC987oMXvJmWd7iWHiGFRvGsF12t/auwdD0cv96p0ruEr7fXuZstmKo/
BB4CHFydylyC1s1Tb6X+KptnvGg++CRQB4Mz623z9qY=
-----END PRIVATE KEY-----)";
    testCertificateInfo.certificate = R"(-----BEGIN CERTIFICATE-----
MIIFJDCCBNGgAwIBAgIUUbKvcmggkC8vzeP+z9JajpNN4JAwCwYJYIZIAWUDBAMC
MG4xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxITAfBgNVBAMMGGludGVybWVk
aWF0ZS1jZXJ0aWZpY2F0ZTAeFw0yNDAxMTcxNTQyMzBaFw0zMjA0MDQxNTQyMzBa
MGYxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxGTAXBgNVBAMMEGxlYWYtY2Vy
dGlmaWNhdGUwggNCMIICNQYHKoZIzjgEATCCAigCggEBAIjpNcjmIoIl+wDjHUX5
mAqdf7SDmOGHxTJJ3xieEnZbxwEwV8qPDtxUkl5db6EzEaM3TtOYwm30YyDuw1rR
BAxESfGd93TFeCrpLbqVmgo9AlbCYIiYEmfDiVOs8F00cCyeDFys5nl4lvIUY5FI
nBXJoxItIGgK2zsXTYiy3NlcxJyWYjFWmz1XMa5nsAiT4F+0zomzx08gyjqaVxkF
e8uciA57z9UDAFdcfnq21e504zrlA9H7xDIaZhftQyBcqs7hbtI2Fj1CghAOKX9v
8YvA8eY9W/Zx25urcnsOUo5Nj/3ZdTz4r2QdZnQIX9HE9BmthtHqkdgBx1ooWMcX
iS8CHQD7i2UEZVwbM+ToNgJZ19yZnTXno2pksS73F9d9AoIBAEZUwws0GGiLMWjz
FX9ne/shFwsCkoRLiCY31wKpE0jFkxCpuXM72M6bSj4H5+0BKSzjKubZUJM55P2f
GSxGTCoR9j9Ik9yAPD1ZplO0CUF20t/w10BJedPMXquZ7WLEnTufEX21PPLYBF4i
vpu9cwwWff18srO1rAfE/DxN/KzwqDoIbGjMGnI/zN9Xum7P+ajafMa5hcaRnner
yIYWkNubxxP4NFN+YkSyE0+PakZSoPP8Kz0MvX1yv29waVAFbMiMFF7YewwXKrdW
pf7rcDRzTxkhj3kML3zugxe8mZZ3uJYeIYVG8awXXa39q7B0PRy/3qnSu4Svt9e5
my2Yqj8DggEFAAKCAQBUDIJ+mKrcMXIv14h1ElE15LF0p4VAEpnQq/EoCjWxGdPw
ZjvlwXbCmoVRZ93fmXERiig5ypnWz9eps7iUzpJy+2LYS3G6/YD2ufns78b7gyCu
ljgWVxND8EgF1j7DLLpdZaZjABqn367yIDpADR6v1J2hP1ByzsHysDbEv+FRRKE+
V8eWVTZV6OySc4YlEAFzSe80i9ueo2MLQjxmAkfrJtoauSAnvRZa8WBoEXx/5sW8
a8JXNGYUzPRE1SC+fvwfBc/khYbvKHiNkqTyx/rIjovFUfadvoAdn2oLbRzXZAEb
RcJD0ou31BCJBrIv8RYN761nz3QgKg3vlUkms90vo2kwZzAlBgNVHREEHjAcgg0q
LmV4YW1wbGUuY29tggtleGFtcGxlLmNvbTAdBgNVHQ4EFgQU9C711fwqRas+phw+
dDUm7KOxRf0wHwYDVR0jBBgwFoAUKmeTpbxVJ4JPxGoK+cQ1FAjpTM0wCwYJYIZI
AWUDBAMCA0AAMD0CHQDBe7LBmX14krryDsPjqhCeztjB/MvB6TQNC+89AhxAaLh5
U8XjN4ZHie7O6FVKuMQizr4/8aFwnVSl
-----END CERTIFICATE-----)";
    testCertificateInfo.certificateChain.append(R"(-----BEGIN CERTIFICATE-----
MIIFJDCCBNGgAwIBAgIUUbKvcmggkC8vzeP+z9JajpNN4JAwCwYJYIZIAWUDBAMC
MG4xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxITAfBgNVBAMMGGludGVybWVk
aWF0ZS1jZXJ0aWZpY2F0ZTAeFw0yNDAxMTcxNTQyMzBaFw0zMjA0MDQxNTQyMzBa
MGYxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxGTAXBgNVBAMMEGxlYWYtY2Vy
dGlmaWNhdGUwggNCMIICNQYHKoZIzjgEATCCAigCggEBAIjpNcjmIoIl+wDjHUX5
mAqdf7SDmOGHxTJJ3xieEnZbxwEwV8qPDtxUkl5db6EzEaM3TtOYwm30YyDuw1rR
BAxESfGd93TFeCrpLbqVmgo9AlbCYIiYEmfDiVOs8F00cCyeDFys5nl4lvIUY5FI
nBXJoxItIGgK2zsXTYiy3NlcxJyWYjFWmz1XMa5nsAiT4F+0zomzx08gyjqaVxkF
e8uciA57z9UDAFdcfnq21e504zrlA9H7xDIaZhftQyBcqs7hbtI2Fj1CghAOKX9v
8YvA8eY9W/Zx25urcnsOUo5Nj/3ZdTz4r2QdZnQIX9HE9BmthtHqkdgBx1ooWMcX
iS8CHQD7i2UEZVwbM+ToNgJZ19yZnTXno2pksS73F9d9AoIBAEZUwws0GGiLMWjz
FX9ne/shFwsCkoRLiCY31wKpE0jFkxCpuXM72M6bSj4H5+0BKSzjKubZUJM55P2f
GSxGTCoR9j9Ik9yAPD1ZplO0CUF20t/w10BJedPMXquZ7WLEnTufEX21PPLYBF4i
vpu9cwwWff18srO1rAfE/DxN/KzwqDoIbGjMGnI/zN9Xum7P+ajafMa5hcaRnner
yIYWkNubxxP4NFN+YkSyE0+PakZSoPP8Kz0MvX1yv29waVAFbMiMFF7YewwXKrdW
pf7rcDRzTxkhj3kML3zugxe8mZZ3uJYeIYVG8awXXa39q7B0PRy/3qnSu4Svt9e5
my2Yqj8DggEFAAKCAQBUDIJ+mKrcMXIv14h1ElE15LF0p4VAEpnQq/EoCjWxGdPw
ZjvlwXbCmoVRZ93fmXERiig5ypnWz9eps7iUzpJy+2LYS3G6/YD2ufns78b7gyCu
ljgWVxND8EgF1j7DLLpdZaZjABqn367yIDpADR6v1J2hP1ByzsHysDbEv+FRRKE+
V8eWVTZV6OySc4YlEAFzSe80i9ueo2MLQjxmAkfrJtoauSAnvRZa8WBoEXx/5sW8
a8JXNGYUzPRE1SC+fvwfBc/khYbvKHiNkqTyx/rIjovFUfadvoAdn2oLbRzXZAEb
RcJD0ou31BCJBrIv8RYN761nz3QgKg3vlUkms90vo2kwZzAlBgNVHREEHjAcgg0q
LmV4YW1wbGUuY29tggtleGFtcGxlLmNvbTAdBgNVHQ4EFgQU9C711fwqRas+phw+
dDUm7KOxRf0wHwYDVR0jBBgwFoAUKmeTpbxVJ4JPxGoK+cQ1FAjpTM0wCwYJYIZI
AWUDBAMCA0AAMD0CHQDBe7LBmX14krryDsPjqhCeztjB/MvB6TQNC+89AhxAaLh5
U8XjN4ZHie7O6FVKuMQizr4/8aFwnVSl
-----END CERTIFICATE-----)");
    testCertificateInfo.certificateChain.append(R"(-----BEGIN CERTIFICATE-----
MIIFJjCCBNSgAwIBAgIUXLcGImGgh6VijMOCHzlpCrZsu60wCwYJYIZIAWUDBAMC
MGkxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxHDAaBgNVBAMME2NhLnJvb3Qt
Y2VydGlmaWNhdGUwHhcNMjQwMTE3MTU0MjAxWhcNMzIwNDA0MTU0MjAxWjBuMQsw
CQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwEQ2l0eTEOMAwGA1UE
CgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMSEwHwYDVQQDDBhpbnRlcm1lZGlhdGUt
Y2VydGlmaWNhdGUwggNCMIICNQYHKoZIzjgEATCCAigCggEBAIjpNcjmIoIl+wDj
HUX5mAqdf7SDmOGHxTJJ3xieEnZbxwEwV8qPDtxUkl5db6EzEaM3TtOYwm30YyDu
w1rRBAxESfGd93TFeCrpLbqVmgo9AlbCYIiYEmfDiVOs8F00cCyeDFys5nl4lvIU
Y5FInBXJoxItIGgK2zsXTYiy3NlcxJyWYjFWmz1XMa5nsAiT4F+0zomzx08gyjqa
VxkFe8uciA57z9UDAFdcfnq21e504zrlA9H7xDIaZhftQyBcqs7hbtI2Fj1CghAO
KX9v8YvA8eY9W/Zx25urcnsOUo5Nj/3ZdTz4r2QdZnQIX9HE9BmthtHqkdgBx1oo
WMcXiS8CHQD7i2UEZVwbM+ToNgJZ19yZnTXno2pksS73F9d9AoIBAEZUwws0GGiL
MWjzFX9ne/shFwsCkoRLiCY31wKpE0jFkxCpuXM72M6bSj4H5+0BKSzjKubZUJM5
5P2fGSxGTCoR9j9Ik9yAPD1ZplO0CUF20t/w10BJedPMXquZ7WLEnTufEX21PPLY
BF4ivpu9cwwWff18srO1rAfE/DxN/KzwqDoIbGjMGnI/zN9Xum7P+ajafMa5hcaR
nneryIYWkNubxxP4NFN+YkSyE0+PakZSoPP8Kz0MvX1yv29waVAFbMiMFF7YewwX
KrdWpf7rcDRzTxkhj3kML3zugxe8mZZ3uJYeIYVG8awXXa39q7B0PRy/3qnSu4Sv
t9e5my2Yqj8DggEFAAKCAQABOoyWJsJRHVrIDTYP4GEXn59LyydMmj0u3RFlXuQN
xwN1Le1976FYb/ZgFI/sITcEReGTxdXJbeiLGyL2S9f7cYVzEdbkDEelzVeZTetI
Ytoe3po9FcAnqPadJUIAUSkaaMhbZjSjjKmgS7iSQdad10jZ/LsJRnBBThujraLA
1XTMJZSiFX/6r6cnwsvjGxlPhjQ9kMrq783dIyk7e9xbH7q4M++URLAn8wwaMPIA
xvXdwSppxWPjETeIm57NDInmvxomMYc1Mx7K0xTbrYJ8kLKoyx80yLflnKII8K9+
gr61FbBnzLci71wUpI9K3wxE+MuUdXDZgF3w5H3d9Z64o2kwZzAlBgNVHREEHjAc
gg0qLmV4YW1wbGUuY29tggtleGFtcGxlLmNvbTAdBgNVHQ4EFgQUKmeTpbxVJ4JP
xGoK+cQ1FAjpTM0wHwYDVR0jBBgwFoAU9hVJnwM/kIU8ifgo1ppfTtRK5VgwCwYJ
YIZIAWUDBAMCAz8AMDwCHA8KC0Wf2jX53p3dGLgXy6pC9DmWSd5+9OOmCTwCHF2L
lHdE5O3nCUFTI0H96jADmqsnFR0jNu23hTI=
-----END CERTIFICATE-----)");
    return testCertificateInfo;
}
*/

static TlsTestCertificateInfo ecDsa()
{
/*
 EC OpenSSL Commands
 ========================================================
 Root CA key: openssl ecparam -name prime256v1 -genkey -noout -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Certificate key: openssl ecparam -name prime256v1 -genkey -noout -out cert.key
 Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha512
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICNzCCAdygAwIBAgIUQ2zCXq++VhMuvR6f38k7QcQOt8swCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDE0MjhaFw0zNTEwMjYyMDE0Mjha
MG8xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxIjAgBgNVBAMMGWVjZHNhLWNh
LnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARVb9pa
4b+6UiH4w/CbJdpOVnKJGOu1Bi3Btp20VUKBroEc6ZoBGt7Ia1owfJz+gobYcypg
1ARALyZp70yq7rsMo1YwVDAdBgNVHQ4EFgQUcialriebkR0zzaW3cYgt66sKvIsw
HwYDVR0jBBgwFoAUcialriebkR0zzaW3cYgt66sKvIswEgYDVR0TAQH/BAgwBgEB
/wIBCjAKBggqhkjOPQQDBANJADBGAiEAtgj5x6nDbxzYzL45TyXafEsonnOG3QMq
9RVDWbw99CUCIQDIH1FEHn4NhcpJbaNy96SRmfmz3R9rXNuVNbGBb5y0lg==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEICE2DE9d2rP+0MVY3TEDKjyWK1afHRKm5CCUKrJ8ybW8oAoGCCqGSM49
AwEHoUQDQgAEA0Rt4RuNf4EhIR+MVqYjjf7yMR69rNf8SbSntD1T7ZsXeeEEm70n
dM5zt2QPjRp6+Kbx9iBoC9xKtb60wOArQA==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICnDCCAkGgAwIBAgIUe+EfVKu8O0zPTO9j7M9IsTfGtOkwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDE0NTZaFw0zNDAzMDUyMDE0NTZa
MGcxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxGjAYBgNVBAMMEWVjZHNhLWNl
cnRpZmljYXRlMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEA0Rt4RuNf4EhIR+M
VqYjjf7yMR69rNf8SbSntD1T7ZsXeeEEm70ndM5zt2QPjRp6+Kbx9iBoC9xKtb60
wOArQKOBwjCBvzB9BgNVHREEdjB0giEqLmlwdjRfaXB2Nl9hZGRyZXNzZXMub25s
b2NhbGhvc3SCH2lwdjRfaXB2Nl9hZGRyZXNzZXMub25sb2NhbGhvc3SHBH8KFDKH
BH8KFDyHBH8KFEaHBH8KFFCHBH8KFFqHEAAAAAAAAAAAAAAAAAAAAAEwHQYDVR0O
BBYEFN7eOuMQimWlq7abNkAy77lGIJxoMB8GA1UdIwQYMBaAFHImpa4nm5EdM82l
t3GILeurCryLMAoGCCqGSM49BAMEA0kAMEYCIQCCl/TMZnnyR2hsK6thXqv7G+gs
BhvwhPPHJg8mgu/rcQIhALSpT5QxltbHxe2c5DjXWXQO9/n1n5QWhX2ruSnlv3Qr
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo ecDsaEncryptedPrivateKey()
{
/*
 As ECDSA, but encrypting private key with password password,
 using the following command: openssl pkcs8 -topk8 -in ec.key -out ec-encrypted.key

 EC OpenSSL Commands
 ========================================================
 Root CA key: openssl ecparam -name prime256v1 -genkey -noout -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Certificate key: openssl ecparam -name prime256v1 -genkey -noout -out cert.key
 Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha512
 Encrypt Private Key: openssl pkcs8 -topk8 -in cert.key -out cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICNjCCAdygAwIBAgIUAaMVTkGZKVYsimiiXDhyXRTkzZQwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDE3NDlaFw0zNTEwMjYyMDE3NDla
MG8xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxIjAgBgNVBAMMGWVjZHNhLWNh
LnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQE2IoV
8KL9rcGyHB99RtupINyI7Mc/AVhmaKWEqwEGBqKyR9uvZw8JqkhkpczfeawqznoL
9D+syVsDzaV+OpbHo1YwVDAdBgNVHQ4EFgQUWRwzJPnhGLUxuT8yzgAiPMimPdww
HwYDVR0jBBgwFoAUWRwzJPnhGLUxuT8yzgAiPMimPdwwEgYDVR0TAQH/BAgwBgEB
/wIBCjAKBggqhkjOPQQDBANIADBFAiEA5JbRf1e08vOcyCLK2+99EJurG1AHxiWu
ZnbNBDSDDg4CIE9XUOCnxozu0UMx4qQVU9/hfUAj915R6+FtX1LAqhcQ
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIAI0ZlYtQ+XCeO8A38azMq5U3GO/i8LHwKW/+UKUVesUoAoGCCqGSM49
AwEHoUQDQgAEd7kVfnIymhlMN5XfHToxcMCZJT9/Rr5XzNO56WPwfgCdaD8gFZil
djdynNhaoZoZQKupCw940xmrsxFlDhSaqg==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIH0MF8GCSqGSIb3DQEFDTBSMDEGCSqGSIb3DQEFDDAkBBAF3iQTnMTeZfZhfHc1
gbVeAgIIADAMBggqhkiG9w0CCQUAMB0GCWCGSAFlAwQBKgQQEzGutuRtJS94Yf9W
fGf3uwSBkO7WMoY9kFKuSzbxNqELb9Q59xbsnWMk/DBrF3PzDrTdzOwMQtw1njeN
XSLCcJcZaQGLHIffwCD14KBKM6BeNAE8fV36VdrAD8IJg+2OY1kmmrgY7dItOOK0
92siFrpjW0Eh1S4oNiDp4o1Vc9DoHVZ+4YaHYlNs7pjkct8aw8zf6ujgM+rN/lm1
Dr5CHaOXJQ==
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICmzCCAkGgAwIBAgIUHap/wPmJ2Sp5FiSUKYpVlAph5GYwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDE4MTRaFw0zNDAzMDUyMDE4MTRa
MGcxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxGjAYBgNVBAMMEWVjZHNhLWNl
cnRpZmljYXRlMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEd7kVfnIymhlMN5Xf
HToxcMCZJT9/Rr5XzNO56WPwfgCdaD8gFZildjdynNhaoZoZQKupCw940xmrsxFl
DhSaqqOBwjCBvzB9BgNVHREEdjB0giEqLmlwdjRfaXB2Nl9hZGRyZXNzZXMub25s
b2NhbGhvc3SCH2lwdjRfaXB2Nl9hZGRyZXNzZXMub25sb2NhbGhvc3SHBH8KFDKH
BH8KFDyHBH8KFEaHBH8KFFCHBH8KFFqHEAAAAAAAAAAAAAAAAAAAAAEwHQYDVR0O
BBYEFN06LhWwgIaYl1ztFFxHXmqQcQJXMB8GA1UdIwQYMBaAFFkcMyT54Ri1Mbk/
Ms4AIjzIpj3cMAoGCCqGSM49BAMEA0gAMEUCIDqL9TzMFzK8FIqRt1lZDqnj/vYM
PcQ2FaM8G+9pnhkEAiEA8NSWBRcD5ewDBwzcM0MwIaMXyLNHe8XnF5tTCsY9pE4=
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo ecDsaChain()
{
/*
 EC Chain OpenSSL Commands
 ========================================================
 Root CA key: openssl ecparam -name prime256v1 -genkey -noout -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Intermediate Certificate key: openssl ecparam -name prime256v1 -genkey -noout -out intermediate-cert.key
 Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-intermediate-certificate"
 Sign Intermediate Certificate: openssl x509 -req -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -extfile <(printf "basicConstraints=critical,CA:TRUE,pathlen:10") -days 3000 -sha256
 Certificate key: openssl ecparam -name prime256v1 -genkey -noout -out leaf-cert.key
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-leaf-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICQjCCAeigAwIBAgIUej0ZMvQDPvkmfGs+xK6yoklTzNgwCgYIKoZIzj0EAwQw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDI5NTZaFw0zNTEwMjYy
MDI5NTZaMHUxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxKDAmBgNVBAMMH2Vj
ZHNhLWNoYWluLWNhLnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAAQVA1+xd9SseZTmB1D4O0DkjH607KW2Bo8CyV9idg4MrdKJ2/cToEUk
aIOatwsLi5JQUYoxp8q9BHvEEvou/AX8o1YwVDAdBgNVHQ4EFgQUIma5Pv5rH4mc
YeA388hQ9ALAi5owHwYDVR0jBBgwFoAUIma5Pv5rH4mcYeA388hQ9ALAi5owEgYD
VR0TAQH/BAgwBgEB/wIBCjAKBggqhkjOPQQDBANIADBFAiEAz0ZWgdYmjwmm5eic
dkVuICcAbuQ3KOPNzePadtZ6YlYCIDwXiP7Mpw5YhXEKFL5rZEIv9mAcmg24260f
AXZwpZJH
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIFmvT/GxWrKjIKjbOhKjzOfKox/wb8psNdMqr57M4w+roAoGCCqGSM49
AwEHoUQDQgAEjJj4oKXVDXw0fHZPfOerUjUeD5ZwHCCbptvTnZKQQHNkRq36CLtj
RqwX84XZbaK2cY3zengG+nYUAVy4nGI49Q==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICsTCCAlegAwIBAgIUHOCxxILsVRtSaWXLrEdWfLzIcHkwCgYIKoZIzj0EAwIw
ejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkZWNkc2EtY2hh
aW4taW50ZXJtZWRpYXRlLWNlcnRpZmljYXRlMB4XDTI1MTIxNzIwMzIzNVoXDTM0
MDMwNTIwMzIzNVowcjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTElMCMGA1UE
AwwcZWNkc2EtY2hhaW4tbGVhZi1jZXJ0aWZpY2F0ZTBZMBMGByqGSM49AgEGCCqG
SM49AwEHA0IABIyY+KCl1Q18NHx2T3znq1I1Hg+WcBwgm6bb052SkEBzZEat+gi7
Y0asF/OF2W2itnGN83p4Bvp2FAFcuJxiOPWjgcIwgb8wfQYDVR0RBHYwdIIhKi5p
cHY0X2lwdjZfYWRkcmVzc2VzLm9ubG9jYWxob3N0gh9pcHY0X2lwdjZfYWRkcmVz
c2VzLm9ubG9jYWxob3N0hwR/ChQyhwR/ChQ8hwR/ChRGhwR/ChRQhwR/ChRahxAA
AAAAAAAAAAAAAAAAAAABMB0GA1UdDgQWBBSYbnfF+MmBx+RJ98iSuSLq+Uh4PjAf
BgNVHSMEGDAWgBRAv5DWjX1mTINc1zoc2MUnI9BApDAKBggqhkjOPQQDAgNIADBF
AiEA9rnnROeVVbjtVo9a5Wp9DUcy530jXnPsv5C4EbbsbccCIBO2oCDfOVKxw2w5
kFu09tRYiT6+eFfc0VhxPCYSZHA9
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICRjCCAe2gAwIBAgIUa2FP7ZgXcqHL2JPWW3UDGDH45YgwCgYIKoZIzj0EAwIw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDMwMzFaFw0zNDAzMDUy
MDMwMzFaMHoxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxLTArBgNVBAMMJGVj
ZHNhLWNoYWluLWludGVybWVkaWF0ZS1jZXJ0aWZpY2F0ZTBZMBMGByqGSM49AgEG
CCqGSM49AwEHA0IABDfY7jn3md3NG7tDFszLCYoK1jelQLbXEhHo/PfRa0ohMt3J
osCQerjKM/RAdVPV0D2HKBL6brGUTCjGaKvOPEWjVjBUMBIGA1UdEwEB/wQIMAYB
Af8CAQowHQYDVR0OBBYEFEC/kNaNfWZMg1zXOhzYxScj0ECkMB8GA1UdIwQYMBaA
FCJmuT7+ax+JnGHgN/PIUPQCwIuaMAoGCCqGSM49BAMCA0cAMEQCIFBB4zH1Z/SB
QhERQyq1kuM2LeI1hhX2x5CWuQoFnUwnAiBkiCpf2+8Zngb6smFumgLWO+oURHb+
VZ/x/D0WFxuF+g==
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo ecDsaChainEncryptedPrivateKey()
{
 /*
 As ECDSA chain, but encrypting private key with password password,
 using the following command: openssl pkcs8 -topk8 -in ec-chain.key -out ec-chain-encrypted.key

 EC Chain OpenSSL Commands
 ========================================================
 Root CA key: openssl ecparam -name prime256v1 -genkey -noout -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Intermediate Certificate key: openssl ecparam -name prime256v1 -genkey -noout -out intermediate-cert.key
 Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-intermediate-certificate-private-key"
 Sign Intermediate Certificate: openssl x509 -req -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -extfile <(printf "basicConstraints=critical,CA:TRUE,pathlen:10") -days 3000 -sha256
 Certificate key: openssl ecparam -name prime256v1 -genkey -noout -out leaf-cert.key
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-leaf-certificate-private-key"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in leaf-cert.key -out leaf-cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICQTCCAeigAwIBAgIUKhyb2RjZFyYPbCdpMQDBU/efX1cwCgYIKoZIzj0EAwQw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDM1MjZaFw0zNTEwMjYy
MDM1MjZaMHUxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxKDAmBgNVBAMMH2Vj
ZHNhLWNoYWluLWNhLnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAAQPYXQ8naOxH2owxLIZoM7HG2G2AvX6VemAuNPB13tYXskdTICstCrH
2wXZQYbZf1e5yqGGiVdcYmv1GbcstUYJo1YwVDAdBgNVHQ4EFgQUnA/sdWI7E+wD
AbIzuJvJ2AWN5jswHwYDVR0jBBgwFoAUnA/sdWI7E+wDAbIzuJvJ2AWN5jswEgYD
VR0TAQH/BAgwBgEB/wIBCjAKBggqhkjOPQQDBANHADBEAiAkqXa9IVCZ4oiipWCx
nJvOiJ/DV8edC9eB5CthaIDJJAIgUJOLxrEdkW1ONpPXUBbUsOJmkPYwNJDmYie9
O/YfUsE=
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIGw908VW8Gcgz5zMhZeg8p2Qiwlt8IM6Ly7UQHRfkD7VoAoGCCqGSM49
AwEHoUQDQgAEMVPnifIdnE+LHx3odyXCAZygtioDQRyOReHJoODDKwrD85ofvelw
n1IXCyWqoAaFpfqTOk8L6igu4FFw4JeQzw==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIH0MF8GCSqGSIb3DQEFDTBSMDEGCSqGSIb3DQEFDDAkBBBG1MJJgDDxEcXZxs+e
kb2FAgIIADAMBggqhkiG9w0CCQUAMB0GCWCGSAFlAwQBKgQQ7C4eUBmkb9B3ydMe
FvmRAgSBkAZZoPRwM2uty5f7/1/oz2sGtBNxLjfb0Lv3K6dOTfz4E8Jg4XkPHXPI
CTfKMTOy6HwrJ4InRcKKc9S1kmOOIhnqJXq0v5l61V+ZggI3uV3xfURn7E9Yh/JK
43sbO7n3DNwpWbzozjoBZc/jHUWNZ1+JU0F8Ql6hYX1/QGaFMm+NEs7XYgwHzHaH
iV+grOwHzA==
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICyjCCAnCgAwIBAgIUDcHny3JgW7eA3mldxrHVGfHLcpgwCgYIKoZIzj0EAwIw
gYYxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxOTA3BgNVBAMMMGVjZHNhLWNo
YWluLWludGVybWVkaWF0ZS1jZXJ0aWZpY2F0ZS1wcml2YXRlLWtleTAeFw0yNTEy
MTcyMDM3MDRaFw0zNDAzMDUyMDM3MDRaMH4xCzAJBgNVBAYTAkNDMQ0wCwYDVQQI
DARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwF
SW5kaWUxMTAvBgNVBAMMKGVjZHNhLWNoYWluLWxlYWYtY2VydGlmaWNhdGUtcHJp
dmF0ZS1rZXkwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQxU+eJ8h2cT4sfHeh3
JcIBnKC2KgNBHI5F4cmg4MMrCsPzmh+96XCfUhcLJaqgBoWl+pM6TwvqKC7gUXDg
l5DPo4HCMIG/MH0GA1UdEQR2MHSCISouaXB2NF9pcHY2X2FkZHJlc3Nlcy5vbmxv
Y2FsaG9zdIIfaXB2NF9pcHY2X2FkZHJlc3Nlcy5vbmxvY2FsaG9zdIcEfwoUMocE
fwoUPIcEfwoURocEfwoUUIcEfwoUWocQAAAAAAAAAAAAAAAAAAAAATAdBgNVHQ4E
FgQUjEFvdZRZxnf2akXWH1q/XD+DNcEwHwYDVR0jBBgwFoAUoYsVDq31QHuyJCvk
rrZZcCfcp54wCgYIKoZIzj0EAwIDSAAwRQIgc3ZDZuVSvBWMRT+DlPHmlT9BOei6
+2Rzg4gjRMeKTfMCIQCi6y1sLVmc9x3RIto3mex53CWRNxQK4EwLxumguKXgHg==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICVDCCAfqgAwIBAgIUaiRVkFN17XJEK/q8r3uGva8AKSswCgYIKoZIzj0EAwIw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMTcyMDM2MzdaFw0zNDAzMDUy
MDM2MzdaMIGGMQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwE
Q2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMTkwNwYDVQQDDDBl
Y2RzYS1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUtcHJpdmF0ZS1rZXkw
WTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQlt7HDe6rt3T9EAiERyeVwxEjLFRO8
JrkXDXisCCsEzvhhhTRA1zqJlNbxv71P5EjebRBp4z2fQnlNPLzCcCIQo1YwVDAS
BgNVHRMBAf8ECDAGAQH/AgEKMB0GA1UdDgQWBBShixUOrfVAe7IkK+SutllwJ9yn
njAfBgNVHSMEGDAWgBScD+x1YjsT7AMBsjO4m8nYBY3mOzAKBggqhkjOPQQDAgNI
ADBFAiBcg5aU3df+wJnKsh3/asLtKMfv8GtkiC4rjEq3EAwpIQIhANPTq+C4L5fg
hNariMl0KUCf9i4QXtaagjyOCBD0FbSs
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

TlsTestCertificateInfo TlsTestCertificates::getTestCertificate(CertificateType type)
{
    switch (type)
    {
        case RSA_2048:
            return rsa2048();
        case RSA_2048_CHAIN:
            return rsa2048Chain();
        case ECDSA:
            return ecDsa();
        case ECDSA_CHAIN:
            return ecDsaChain();
        case RSA_2048_EncryptedPrivateKey:
            return rsa2048_EncryptedPrivateKey();
        case RSA_2048_CHAIN_EncryptedPrivateKey:
            return rsa2048Chain_EncryptedPrivateKey();
        case ECDSA_EncryptedPrivateKey:
            return ecDsaEncryptedPrivateKey();
        case ECDSA_CHAIN_EncryptedPrivateKey:
            return ecDsaChainEncryptedPrivateKey();
        default:
            Q_UNREACHABLE();
    }
}

void TlsTestCertificates::getFilesFromCertificateType(CertificateType certType, std::string &certificateFile, std::string &privateKeyFile, std::string &caCertificateFile)
{
    switch (certType)
    {
        case CertificateType::RSA_2048:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.privateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
        case CertificateType::RSA_2048_CHAIN:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048_CHAIN);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.privateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
        case CertificateType::RSA_2048_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048_EncryptedPrivateKey);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.encryptedPrivateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
        case CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.encryptedPrivateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
        case CertificateType::ECDSA:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.privateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
        case CertificateType::ECDSA_CHAIN:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA_CHAIN);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.privateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
        case CertificateType::ECDSA_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA_EncryptedPrivateKey);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.encryptedPrivateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
        case CertificateType::ECDSA_CHAIN_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA_CHAIN_EncryptedPrivateKey);
            const static auto staticCertificateFile = createTempFile(certInfo.certificateChain);
            const static auto staticPrivateKeyFile = createTempFile(certInfo.encryptedPrivateKey);
            const static auto staticCACertificateFile = createTempFile(certInfo.caCertificate);
            certificateFile = staticCertificateFile;
            privateKeyFile = staticPrivateKeyFile;
            caCertificateFile = staticCACertificateFile;
            return;
        }
    }
}

void TlsTestCertificates::getContentsFromCertificateType(CertificateType certType, std::string &certificateContents, std::string &privateKeyContents, std::string &privateKeyPassword, std::string &caCertificateContents)
{
    switch (certType)
    {
        case CertificateType::RSA_2048:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
        case CertificateType::RSA_2048_CHAIN:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048_CHAIN);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
        case CertificateType::RSA_2048_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048_EncryptedPrivateKey);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
        case CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::RSA_2048_CHAIN_EncryptedPrivateKey);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
        case CertificateType::ECDSA:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
        case CertificateType::ECDSA_CHAIN:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA_CHAIN);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
        case CertificateType::ECDSA_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA_EncryptedPrivateKey);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
        case CertificateType::ECDSA_CHAIN_EncryptedPrivateKey:
        {
            const static auto certInfo = TlsTestCertificates::getTestCertificate(TlsTestCertificates::CertificateType::ECDSA_CHAIN_EncryptedPrivateKey);
            certificateContents = certInfo.certificateChain;
            privateKeyContents = certInfo.privateKey;
            privateKeyPassword = certInfo.encryptedPrivateKeyPassword;
            caCertificateContents = certInfo.caCertificate;
            return;
        }
    }
}

std::string TlsTestCertificates::createTempFile(const std::string &contents)
{
    static std::forward_list<std::shared_ptr<QTemporaryFile>> files;
    files.push_front(std::shared_ptr<QTemporaryFile>(new QTemporaryFile));
    auto &tmpFile = *(files.front().get());
    if (!tmpFile.open())
        abort();
    tmpFile.write(contents.data(), contents.size());
    tmpFile.flush();
    return tmpFile.fileName().toStdString();
}

}
