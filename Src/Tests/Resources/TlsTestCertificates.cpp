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
 Create Intermediate Certificate Extensions File: touch openssl-ext.cnf; echo "[ v3_ca ]" > openssl-ext.cnf; echo " basicConstraints = critical,CA:TRUE,pathlen:10" >> openssl-ext.cnf
 Sign Intermediate Certificate: openssl x509 -req -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -extfile openssl-ext.cnf -days 3000 -sha256 -extensions v3_ca 
 Certificate key: openssl genrsa -out leaf-cert.key 2048
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-leaf-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.ipv4_ipv6_addresses.onlocalhost,DNS:ipv4_ipv6_addresses.onlocalhost,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID0jCCArqgAwIBAgIUCjU+RegeBep9wA6p7WF1rM82GL4wDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzE0NTczMFoXDTM1
MTAyNjE0NTczMFowdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UE
AwwhcnNhMjA0OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAr3yZrVktqRuWKi+kx2H9V/Kmh5jRpD0YsK1s
vA+6w+h2gcf2ybVFzPmNGdKOez0VRFiKI+yT7DM9tv0GNo2oKqDLFAxfCS4r+qAi
JTGmRQijduLD54/dS7RcJX30XBKWiH3C6w7xq+63dJeCefYTTxLereM+QNPrafas
/kjUKa1PVDJKYBODlHAHQuRnQ7MDu0J2y2bYeY/XEjl9lC1YJuP4sA26wvMhMfjW
NYorTJzU59iKYjL60KIqY5rbrAMravvQ3CKpCzanJZlyir83KD5K412SBJ9Ywpvw
CR0Bqmn59ihnX/C+E/QTCkJaXqTzdbeB66ZHxDe/BkDiq3MeRwIDAQABo1YwVDAd
BgNVHQ4EFgQUxcaTFPwt91QSbOuhIh2DxY/yaJkwHwYDVR0jBBgwFoAUxcaTFPwt
91QSbOuhIh2DxY/yaJkwEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG9w0BAQsF
AAOCAQEAYGMfauB3+wWGvTHHXI2q4Xv4sIUePpbC4OnkU2+JTX830vRwOoGOhHG4
3A+tDRGxd/xT+zjJOwK1kAiU9OuQJkP/PrmhE2WeqiXZ94cCFGnlqTDpYdO/yvw0
NXzKHaJAV8PGbbbxlFprBixmYmr7NbCPQvaq4bfAqd6vHdtNaTcVEWE1eB0RiCJL
gzJkMlZXOWxcSfDnDF/Vy8+sEQgD69Ne1sTHWm5VZI1pBQogeVd/BXAU1DVY4g+P
Q5Bu/EbJbgx4/44h3n9jPpeu7qQH0eQmqkxtcIX7WbtNwOBsJJtRy3LKWwUJNXTf
Z8X+VX6hcu15UwCL/vT9rOMxT89N2A==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDqlsQxlL3FAVLD
bwq3j6QOKYaxO+zAyJy3A98TCp5MLrfwAVnnm/izGN5xBG88vMqQ4DUqqDN5enYy
Yp2CF51+cdELCB7L4Gy3d7u6QcwzFWbUlpeP44zJ+B7Swjx4rjpq5dXjXy72Am59
oW9yGSPJ24a9If4UwzPvmOQNJdH9k6E4EAJDBnto9IFFwF3GoyKfER7tHlbrGZKe
QUBFBYE9ciwxeTqKVsnfyBUuyAxUJDmjlq+HhamvkSuOUDRsJako1a8wgGM0yxri
sxRv9l+tZOVDs/cw0ZBNeC/lcMoBbJm2kRc1KcpvSdPlyEzHsZd0Sg3COGqd8Zkn
FdJK06xPAgMBAAECggEAC8p0JgHR3/sHC+ei5EMF/eg8auAWMEJlDhXVuoFIH5Ah
kqCewjNmtqnDVG7ioGzkyY9bXL+DhtGO601lmUpSki5qUWGhGqI3FKmh9+SivNaQ
ipLfH05SnzwnYHR+0ZoAOas/quP32kmZABuDHHsC/3xXM15oBOcizyIiJD8IZPRT
/V19Z3nHMed2WkKl4EXepdJcpTckS7vyvIkf5DFiM1zX1xkAtiLj6her+DPPaNT1
3wJ6tx1GMpDGLO+E2Y4LdhWuUI6OHsuUEsHNv82uqMtI0ALyAi32qpCr8clGZUwU
n4twQgWcupZrwTPNWQpxQ+HFqElGeP3Ab6KuJPyyoQKBgQD3SDgy+e2VlrS2e31O
NS3muPQzOHiFYHHhtn3fCiD9e1kKBDnHRbTlM/V6Ldo0FFu++fTgEkiv48m2H+cU
XfLcQveMddIJ1XerNcbJQs3iE9bXvplu/mqXz909xVOt62SpDBZPK6pPNkaSbDnJ
Z6yZgqEMn7YO50nYWu9OUTt0FwKBgQDy2/zmDy8SnGr7njJDaWAtx0IwSPLL4L8g
AMyEaUSypCx4Iux2mV/8OIXQ8vUGXPnaQcZb2LeC5r/I7AZw9AQXVz7wqTRvpmH6
w1CqLVuXrP0QN4+ShgcoUsHDxT7TJJrU31KKD5Hrbjy9YpY9/GcMEDM4nsi2AAC0
uKJp401UiQKBgQC71foKjtqwBHhlHUzDuZWSau/kcILsHaSj5bGbWOBSa/3oo+kX
u5V7fYp8MpRLdyNx4ADorDrWkNtKItWvHOY79gqAgYb4vVRXk1Pa0Yn+rz9FRpza
ORvTEaxnHZtAw0wFNQ9FOE/4N6xH0TncCYqMWTPomPD8RShM1lZtUTsuJQKBgGvz
Qx/UAf2Iz7kWoYr7bQgMOSyTUHE1bl9AW6hDzoiFl0qzY154YIURbtHpHOb+LwVI
FW7ADxo9jWQ3HcnZfXQOkOiSnb8/OvAnH1T1om/yMe6ic8wSjV3sIzrtEzbxX21d
8ydxMlBvCMq55sSYzAk9rjbtxZO4vpeWNoNQyQ/ZAoGAckLF89QJUZV7uHUinH5T
aHOXcm8JQRIsg0NFlX37OVT9mIc/YbhOAVqBqFHB7lFtKOxI7OaliCiiwPdc6u1Q
GWej1fZoFdvwbXsioO1H0nON/VyvGfDxmADB6FRObUqL4VoERFSLGqcdKL3dcSAZ
xBcMXYkLBHZXOJ0P4bSvlDM=
-----END PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIEQTCCAymgAwIBAgIUaRpULk5Vzleu5PAjY/mlSkrtrEwwDQYJKoZIhvcNAQEL
BQAwfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UEAwwmcnNhMjA0
OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwHhcNMjUxMjE3MTQ1OTM0
WhcNMzQwMzA1MTQ1OTM0WjB0MQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTEN
MAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMScw
JQYDVQQDDB5yc2EyMDQ4LWNoYWluLWxlYWYtY2VydGlmaWNhdGUwggEiMA0GCSqG
SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDqlsQxlL3FAVLDbwq3j6QOKYaxO+zAyJy3
A98TCp5MLrfwAVnnm/izGN5xBG88vMqQ4DUqqDN5enYyYp2CF51+cdELCB7L4Gy3
d7u6QcwzFWbUlpeP44zJ+B7Swjx4rjpq5dXjXy72Am59oW9yGSPJ24a9If4UwzPv
mOQNJdH9k6E4EAJDBnto9IFFwF3GoyKfER7tHlbrGZKeQUBFBYE9ciwxeTqKVsnf
yBUuyAxUJDmjlq+HhamvkSuOUDRsJako1a8wgGM0yxrisxRv9l+tZOVDs/cw0ZBN
eC/lcMoBbJm2kRc1KcpvSdPlyEzHsZd0Sg3COGqd8ZknFdJK06xPAgMBAAGjgcIw
gb8wfQYDVR0RBHYwdIIhKi5pcHY0X2lwdjZfYWRkcmVzc2VzLm9ubG9jYWxob3N0
gh9pcHY0X2lwdjZfYWRkcmVzc2VzLm9ubG9jYWxob3N0hwR/ChQyhwR/ChQ8hwR/
ChRGhwR/ChRQhwR/ChRahxAAAAAAAAAAAAAAAAAAAAABMB0GA1UdDgQWBBQnMB/O
g9D1iL4e63k+gpzCIorb2TAfBgNVHSMEGDAWgBQ5/GvsV5ZX0D2zg6M6cuAAKnaU
4jANBgkqhkiG9w0BAQsFAAOCAQEAfhwGqEKvYk83PEsOH+JgmrqFAJoj/wHVDnVF
41yNP9yTqBXJVucC4TEB/6o5ae+jmm55aKPsj6fS30qt7NV6DfQxBItpvez6gjkW
KJUEj0QWNYfQh99olCa0i0b5nyrmn9/zqKTEOyuozoUoJ4TWzAhikl91U5VxNxAf
tS1QuOSm7vnUSa6UB+vJI5wdfc9+bwC7UOh7vQOBJQ7gHj1HiW7VDhWaSWHGrGno
Pa7FZuI7ZvJOc2Dd/SRwLzo4X1DY3f+1N8nWJaB75SEoONw+sN56OEB4mmkBbbjQ
bMHLqpoPQ+COQHtRE8voKelx+Sx2+2B+ReC1ijcvQyJQcNHLUw==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIID1zCCAr+gAwIBAgIUMXbV7F7AoO+Ogr8yjCAWVF47qbYwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIxNzE0NTgxMloXDTM0
MDMwNTE0NTgxMlowfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UE
AwwmcnNhMjA0OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwggEiMA0G
CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCuxQyXLEvS2bDQ1TV6OUpegAwdM51S
vgMz5Ak7vffvIu+FquWuKAckzHv+/aSOq6+24iKnovX/071XSUSMcpyfnH3tEZkf
v64+vwm8mlenM7BDtznwSKtbNb/TBZOBT7sijShkiHFd4hGR9R6t6lwcPn/k5/v7
aieegf00eP5UUWPILNh3BlqIc4oyL2igJ2I37fDzyg7JTwS7NMnFLegavC1um7Y1
1tuUhKa/MiTo8wKidz9mqjzpxsPDHNCS5G4dtB1xzagnIt+YkD14+lqvqWS8uAOi
KRaGPAAgKSg0hc8u211Nh1X9pwQ0ERQbDgW1lfVMLW9F+/8mb7VFkvqTAgMBAAGj
VjBUMBIGA1UdEwEB/wQIMAYBAf8CAQowHQYDVR0OBBYEFDn8a+xXllfQPbODozpy
4AAqdpTiMB8GA1UdIwQYMBaAFMXGkxT8LfdUEmzroSIdg8WP8miZMA0GCSqGSIb3
DQEBCwUAA4IBAQB6xUuZlkJEJySD9QxWfVI32bV3nqvbq3WgvzchAYqguxVhvirR
TL+tawfjeqpzi7oGl3GqqAU8uRTVD4T0tjCt1kJYxrMN5nzUBdOx1CPf0tV1igTL
1vQGf7ZSZOxIpTSr0ndGoa4ydXarPuHlggfvZyZqZ/A2qAly98WHcayf3fhS9ngQ
treD7cqOiPuv58xjXvpOHomeEgEaL+eORgwdCx3NQ6GsnKaWdZd01Kt8iRRgNBek
8W43AV3D04ZSJOvxNuZsq47uU8+rGRoPH8JNkk+T4bg+hee/qcaPd5ZCHH8UIOt/
ef89H5Wyi/DLxHChH3N5//mkUFxMELzTCLPt
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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in cert.key -out cert-encrypted.key

*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID2DCCAsCgAwIBAgIUMrucm7a2Rh1BiCU893+/oWOjxagwDQYJKoZIhvcNAQEL
BQAwejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkcnNhMjA0
OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI0MDgyMzE2MzEzNloX
DTM0MDcwMjE2MzEzNlowejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTAL
BgNVBAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsG
A1UEAwwkcnNhMjA0OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMIIBIjAN
BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxXtCEj/dQJmXdBwgYKxu2LbH6tFQ
hPtDyvmioLcpKQnzCrKDoeedKiFS3j8p5TfJLM70I+OwYZcnpN/FjoPLFLbry7Zh
QWuLi/btAN9voxzM8xQpp3GxyDX+mSjNqPRxNocWgvCht/cCY47BKPOu6HrcfjTo
sQaSJfG1mgNCH5DxEB28aAzPg6pel8fgL/hCVyoSDcnZkdISXrU2v4RdigOkjSS/
RO4Mx72zxcf+ebnnI982ZyPKMzgPgeIG+s90aW0TRTO4E0gj+8XQa+/xe5N79cUe
uF3kAI+dai8dfv8HM/h6WTdxJ2mMbgM1tSCarkDhGFdeFfTNrAvsZuNMzQIDAQAB
o1YwVDAdBgNVHQ4EFgQUiwlaLe2Tfd5iEi2SNYgiUss3FrUwHwYDVR0jBBgwFoAU
iwlaLe2Tfd5iEi2SNYgiUss3FrUwEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG
9w0BAQsFAAOCAQEACdZ8NXmzbhzuZUynRuRT2j7QZmXPWA61gzNEIyvbraegV6hc
z8YHEZrbMNPqmk2EYuVJ2JPO3/HNDgXYbEsM2HrZ3AcHGjmChwfd48gMtiP7Yiat
gQWT5HD2mg5xrtFy7+2N/3QGoIDOvFmhw3E7WcZ95yNlvUrOQrrP8Nk7lNPoFOsJ
R6MHCykvyoPhHPSd8emc8cLCcWi+oq5mqUGoko2338EfPA9t0+nKpLYQ3gyAdsN5
Hl3LNGHMc8X6GGqrJYD14mQxjoNSq1qKU2g40O6q4b173wXRDbgOaLsvslTeA094
Ca6K8xHp84xob/GfheGGqr3O9G0dKEzlgP0j/A==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCflDc6HtaO+Qcb
G09mr4m/00R8QyjQxqy399hjnNrcdRz0LZzXVIQzq6ua1adlGk8xAeIeL4zUuiBe
PKo6CLHWjfFUXvtc7VPtNVzBZmQNmbK/q1YC2McWh+rtdxmd9hmEfo+yexckXjl7
8jaPE33Ke+GmmEd20FcNPm3WuSg2oCfM0fNiLPZQiPoqtLdWxN/7WUkBBldofRrE
SJpJV8c/kkQ0XUc3S42EKa63m0nJ93YvL2CJk3T4Hl0N1jUd9wowg0KcmPxkGqv5
Q6D/xEKtmPWw5Vu1T/cbO2NjEeHUVvu9+3AdNZYpelFnoUWLVsKs5Av6VNfkI3lR
9D03spO1AgMBAAECggEAIhLyGu8wRb2PYp6ZfPXg/iyEnFrzyWaOxUZ+9kAtLHM2
UD44FfOGOgk/PnO+B1TPkYmTKHpjQh4PIMgn2fX20TckT21rk1tatxUUnfg4x32o
4Qva9QcoNZumkneCqQu+RaAHm2b0mU+kSx68Px88liKPG3U7FzfjyqBosDd/inAg
S6GA73gfYeRGSFZtnOs/clWH6fGA0pGHCDrEMB1vUD0SGTvYncYs23ag7xP5YfDk
iSnpnzuNCShWkO35DAwofRW6gXy2RIyUTpELI40DKuwBGkgRRTXV3Rll/xw4n3rx
GPGzW2GfkhwUMr3V7CliZshY2/pF2+GyOhk565TQmQKBgQDQkhBULWAAnE3nz92L
7LIzkeUrEKDDz7ctXElVirrJazTGs+bqU1gxL41hrWIj2lcrPYrq/d21iPD4CAwi
N5rV2RFObLJJx4LKCAtNT3gCAmLmJg5Zs6L9a3fsO9wmdimozacpzAzNF51PxPrc
eINSMnuVsaE85BrMJGJU8epTPQKBgQDD3hsnz8257Ib89UYKYgo3C8jlOVKBQFMy
CR1zS6glxDkUScKVHU5GNxpPokZGVzN41nC7iRTk2JWbL5cUfuCIV7cvdb0XvqCV
fnlx3gVJYei1i4EmLXgkKV4IhCS1spokyjxi8gk74xZyt7qCAhYDo+YK2OQ7zO75
5avZAoZp2QKBgASaFG4nCiU5Taa9uV+q+0uT/oByv3lSjfIcRSn0A6cKDIIN2hx1
dk6vg+kR0EaGPMIhe30v2MJ2oQp38vyWSX5Kac1XLJ/gHQykHMu54yX4M4JseIiW
JBR/WIgH0hWvKcAluXh8nhOwX7Iw4HAY4JzhrERjRPF4/vZeHyVMqSTpAoGAGfHM
tQxQmuUayF0Q5wUvhzUXak2agSZtHr6YWRJXdpE0YlyW2rukB6b65DuBhYW9eq8l
BaJV0y5hOsomAAACa+z6hd3Zq1CD4ul62rtnBd5jPdD5zX3OYTPdkdE3L541ztO/
Bfg7guREr/NoUBpIojjOmvc53DX8HbvvdHJiLZkCgYEAht+8c6bcBXWqizEnfhLo
C4Vhz/Hyikx+oYf/Xu2x6hNE4muDIgf7D2vlhn2m3+bGnaJC1ViQDTkAQvIUIL5k
YRLapsxfA4BTuPtsEXu6tlGLfmaHXgrDZybCIxCiNda1NisuEZGnPilDmyBvYNkG
lk806u/3oWdNzoZS1rumnkQ=
-----END PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFLTBXBgkqhkiG9w0BBQ0wSjApBgkqhkiG9w0BBQwwHAQITsjpK6cbscwCAggA
MAwGCCqGSIb3DQIJBQAwHQYJYIZIAWUDBAEqBBDHJ+VxDT9gYBewN2PWAmOZBIIE
0JJ+eda02OQB6sFo0yQs3/I4rzRH2cdtKdTLLN0zE9x5lT/VMGz/Q3OU4VvLA0+0
i5WRYFtjIqTVM1MG4F5OmpLWRuHPykI8AKXpyFU4i7Ie06bkG2h0gwvM3K/qDbY9
4Ar8knMzyjt3ftNudMkHAx1iP3bXNnQrkUWtpotc6ljQMXKe0XjamoOG2dvNgcvd
7jC0PqdAvljnL3CdlfpdZrGNk7tnjj+oyMyhdiFBr5/o0bR5MGiAV1YBzbdLQ7Ru
KFD63ExiET1Xf8EQ0hRJdSiYehd/8LqJVBabUmnNWB5+/1kISHlVspdGIoXZDLmO
cOwOR+hk5JVW5ATP5Y/n81gaJxBEtG+4eKMeyH8jaDTgsDYmtEeaM5ya+VPV1/mN
kjZijgG7QNM28YD1XbXTOIjPBFuFEX/ae0tgP9wSQQah1IquMJUhFaqi49PHDD61
KRuHNiH3f807n7JsGZo+6UMoJI6yH6mYkF4/cUxGdwJ+OOqm7Qg8cRSl5ifeNcO0
M5pWYAW3YY62XIq33VbpfGucGgjwU7D3ogut7LUxn8G23GOaW6H3qPf/MGoplqwB
RMePuaqO6UFq9h73YdJ94q9m/IQ9VF8yAGn03ms8IvDuFuXSODlfrmuyI1wi7HFy
gp+6xUeLrHnGjj6KIruU5NzrlLIXZoY6V/gJAyw2gSo3zHOmDOsg8wQ72tTziN1n
W9EjmEtQ0tWCYJsS1sGRoc0nCrBgVJNlKm1h5RxNenS2LxGfWBV7aDvCz7oGqQtR
+Eo4OCjI7yyWfCbQkHu9f/mhZZB74sqGh/6Opq6IPElyI2KpTG4t3YIfvp4WPzf+
Vp78XJk/UfJsd+/dvpmsqhxESpPRfH2Hqo6ENNJBpSnbufYdbsWmVSki0aNHswcd
MVyMPllb3rs/eNfh7nrhHfmA9z44aY/X++yDDy0oOCmQTFCLQbfGOBm43xkFBAwX
vxotxHl57dGv5hrOBaovj/kot2bMxN+FOh36h/svogbC+QyTDkcU+R4d63fmJ7hq
HVe6ruRuIO/nqxxlFf33mbCsQty7YS2mSSnLSIifd0U/wUT848AJZxcZxoQf2RNu
DNgSu1QVxyM7JtdDnIJ/wT2vpXioZIqa94UpJQomprDTxg7jJJOcWJhHOGBr4hQ7
1a7xYgVdmX7nNE1w+IGahr/0UN/5m8RSVIxVJmLUVvEN3koo15z38Lyhl7d6toXj
3zbBdMZmW3tl5fJcaInte6XWx4q9WebsWX9hKJrasX2KJ/4Yuvcs7ZDR7lv6QZAu
gGc/rjTs0Gwo3GC1ZW21yN6Xsa/hpLTKXCUAwJ6RmW2LB40QY9dZ09tzbrdyrdA9
RHBjZ4dhZeGkqMDhCfkrDJkQ6FFuL5K3fVlGwVBmBmxITPzRVBFRDEIiqGpOt1SB
EbFoU32StCc7D+NWTbl5z1nu4RZe28Q3LF4W5wDSkZ9U9cQVDvSs3fOuSxoqHO8W
GmYQBN7QukSzs+6RlvBljYfmIEkip2mAZToKvIwEkFcnO0npTxiEfk3MhLwUf8Vw
p6l8Ta2Gj8i5s8JuHj+IA8B5GBZdzrrG8JW1PdMiBAdZm6U5tSLaQ88yZMCB/Wem
7K5OMVo6mNaQV8rIrG2RQzz8RIPvaw06rnNzFvR/8HOt
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIID9TCCAt2gAwIBAgIUDSP9uxuPcVUai9iJDq0pG7wWd/YwDQYJKoZIhvcNAQEL
BQAwejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkcnNhMjA0
OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI0MDgyMzE2MzIwM1oX
DTMyMTEwOTE2MzIwM1owcjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTAL
BgNVBAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTElMCMG
A1UEAwwccnNhMjA0OC1lbmMtcGtleS1jZXJ0aWZpY2F0ZTCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBAJ+UNzoe1o75BxsbT2avib/TRHxDKNDGrLf32GOc
2tx1HPQtnNdUhDOrq5rVp2UaTzEB4h4vjNS6IF48qjoIsdaN8VRe+1ztU+01XMFm
ZA2Zsr+rVgLYxxaH6u13GZ32GYR+j7J7FyReOXvyNo8Tfcp74aaYR3bQVw0+bda5
KDagJ8zR82Is9lCI+iq0t1bE3/tZSQEGV2h9GsRImklXxz+SRDRdRzdLjYQprreb
Scn3di8vYImTdPgeXQ3WNR33CjCDQpyY/GQaq/lDoP/EQq2Y9bDlW7VP9xs7Y2MR
4dRW+737cB01lil6UWehRYtWwqzkC/pU1+QjeVH0PTeyk7UCAwEAAaN7MHkwNwYD
VR0RBDAwLoIWKi50ZXN0Lm9ubG9jYWxob3N0LmNvbYIUdGVzdC5vbmxvY2FsaG9z
dC5jb20wHQYDVR0OBBYEFAsZoETi/fC8NUxr09NA9qewVnbiMB8GA1UdIwQYMBaA
FIsJWi3tk33eYhItkjWIIlLLNxa1MA0GCSqGSIb3DQEBCwUAA4IBAQBpp2gGrEmf
ZCFgBUyM47Fq6c0BXPmveDUYSyZiBDSCtS0AtXZ0kV/kE7PUbmZwWuYCiBXipOYk
gxZGIHZov3bj+xg5E/YreIwACEMLKosW8mGwwRKtpsQxeBDbRNy9CYr+1RB6m5cz
J8R0EFGaZvngIFIb/peV7Xd7ZCrfKGbbXf5e6c8w88iazMjqsrGAPqNfhDzTP5Na
qnwB1MvIOL7vCGWXqbitx9BKl6rqkWr88bEacu7Em8YOgyQ60SrTRaS5I71EgPNr
QD8OWmfGGLnwzuISYFSowe3wVwa+KmfrzB7aJS+b5smWVZqWlrVXxaR3glWgUXJE
7sJ2n1EwypoS
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo rsa2048Chain_EncryptedPrivateKey()
{
/*
 As RSA 2048 with intermediate certificate, but encrypting private key with password password,
 using the following command: openssl pkcs8 -topk8 -in rsa2048chain.key -out rsa2048chain-encrypted.key

 Root CA key: openssl genrsa -out ca.key 2048
 CA Certificate: openssl req -x509 -new -key ca.key -sha256 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-enc-pkey-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Intermediate Certificate key: openssl genrsa -out intermediate-cert.key 2048
 Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-enc-pkey-intermediate-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Sign Intermediate Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -days 3000 -sha256 -copy_extensions=copyall
 Certificate key: openssl genrsa -out leaf-cert.key 2048
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=rsa2048-chain-enc-pkey-leaf-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in leaf-cert.key -out leaf-cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID5jCCAs6gAwIBAgIUMTyNBtz6OCUgxGzqn675DRNVrRMwDQYJKoZIhvcNAQEL
BQAwgYAxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5
MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxMzAxBgNVBAMMKnJzYTIw
NDgtY2hhaW4tZW5jLXBrZXktY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMx
NjM0NDFaFw0zNDA3MDIxNjM0NDFaMIGAMQswCQYDVQQGEwJDQzENMAsGA1UECAwE
Q2l0eTENMAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUlu
ZGllMTMwMQYDVQQDDCpyc2EyMDQ4LWNoYWluLWVuYy1wa2V5LWNhLnJvb3QtY2Vy
dGlmaWNhdGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDjPOgpexe5
H4cKOKHLB3O1Ga23bexjs9+2v7Z4+CchrFGPcgSreRsYWtLFpq/6SRtoy/HH3VBL
/zJTO9OVExU3zFvBSTX7syor2NU8WjkBUoJkyFqgt3VGmT18PNVl6MbHdo4U0eJa
FBWElT6Z1CJYRUNrFqUidLgTZMMLxt/1CStqMkVf1bNF03oCXmxd/Zg8wtu93ltZ
Jbs3A6SA+ac6MiB7vbj+DjNXvyvKUKEqzRk1OCweP22ICtuPGV4kC0nTRnHPcwlq
Gn5iLIaoUjgIeklWZMtPhh3Ndbop4KLxAoKSfAeFeLmRAQCPkLIJ3s8+ZHViTQMs
xh7goiJXH66bAgMBAAGjVjBUMB0GA1UdDgQWBBRRc1rxiXEpO/ozCbYXEBuMRQf5
cjAfBgNVHSMEGDAWgBRRc1rxiXEpO/ozCbYXEBuMRQf5cjASBgNVHRMBAf8ECDAG
AQH/AgEKMA0GCSqGSIb3DQEBCwUAA4IBAQBFW3h1cNPvyuPlp8gfKE/j/1pVLDfM
5OOdPW5/J7MuD3S73pAFWTYQTS9VKDys9mPIFa+pUs2B3s0lK6AxSIxrRbUJP4xb
AroDj5KgJN0qEOXumBUT3ds9xzlrylQ4l6D9Y5FPufGfzhWQyKk1D+L+obw2pgJN
VEbyjlaziXMonAgx93tiAk9ZTdpgLKOJufGGr2m5/oK0iIOehpeNYBcPLvwKbqSd
avVmkr0eFea07YaUdMVI8ivkJvSIYNnHI43U/vrHGdBX13EYaP9hgXufAaasMfWh
rCqqLkK2S+VHa/KXwmmamQJJLykVFshipBesFeoNldtI+m+OpM6CWIS/
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQC4VPc8Vn2Z+eGe
goeqyK5I7WYl1mBufpxinW+hkjkxdq0zxbUGp6MSvyGwoMxITDyk8mehufUKaadP
dA6svXqEwMMym5DrWXA8BqD6jBfjdvIPha1eXKleWDkE1nHosCEMcZ7GrjxrbOSt
RKbGfszOQWgIpEJWn8jSsBL+w80VZcUhQ8/KalK/70C/quMT3n91l+JpuusrK9ie
eQDlNB2BKIpn8+o/o1G7LWE0sgaD2VsqX/aeBxg/+56P0wnWAPF4+/68LP1c4Xk1
k6NjDGRAYvXVlkWNXiFzZ38LQtSceqMuj0Jd3GyuUDiYapY9c3Az3Hp0VWOmieSO
xxVMvgNLAgMBAAECggEAORXXG1CNxUmlnbk+2p0bnGoo4D7rzkxXkhK/p4HpRHBE
Y2wvC8iOsBeRD2o82YcCoPKRNXypbngZ4HlIsiZb8xDfYqTa1If2QiCraexI2aDU
hgoYDskTiqw3vE/NJwqksX3edhkvjPVEPISuQXqzYozO8x4rjTgesWDmARASSzWY
9pSw2yefpEgY3sWFStOIYz6dUt+h55spw9rJ5RB8djIieT9Qn4iRAf4rms8WEt39
gC7Hu+NlQIvIUdPE+ThAzJMH/PYU7annWEX+qONQFJTgRIdVvI8rFgVvLlCSW7DW
B7tBp5JBUVUCQBkrtfH6wjR/mqklyngwP5drGf3KwQKBgQD/BSbuFZ1zC7Lype4L
oCHKa4EYta1kSIFTeRTmJYLqWlpjjUx9fe14wEmIwAy/7aJcrPEo1ON7HRJ2Qsog
IeSbi6L3vq8NoseR4CZBHnsjgw8spKjeD0WSuzY55hYpIEAKopzlGHSl49blRmlW
EpxZhIywqQjHj0/nQigP0ivxNQKBgQC5CkgtfTHYl2EsAiVUXrz4KF+iojsTjw0w
f3Fz2eXakn2E2zykXuhlG2RuSiFHXpBkh5IFc9Vlloli21zICeqQMLR99YtvH+4/
s03QRSXgRjHryMv5wguLhqQJQqRv/13+DTEcWj5kw++kyda4j19Nuh44ZfrEz7WQ
09cXbCEyfwKBgBJ/uy02IC/CP37ejoAFY64dUkYKl8NYhxkZBW0Ud4SsfTYPf+by
hZFm0W68D7C0ejyixvOhTccWespdWfAuxTiLTo0OJgClODjau6upnGEdFrK0VxOU
pAVa0zor+JuPHVYoKZ1swrlt557GFsoJm5HazaHbASoIuVEbOXC8XDepAoGACqch
4qSLgxmr+XmiJopRvMOuRdGrLwQUUU90+6N2zS+T1Fs+0YO5Q2DhVkwkQFScYX1A
ldnGVlPlUvCoFcgkXeq/6WSCg4JGGBq9hxkEBjZkV1u3Coj2WzEELphMmToNcjvW
MPitEOYK3OTV3Mg8R2BIrxyH5F7E6DKZ9no+KwcCgYAxexigcUU6uNP4fgpBATfC
jOmlpyjbn3oCik/Y2Qf7jZxNDY6sjWvPUPPv6t+uANxybS0L8tqdnqxpcKI2oaaN
3C9KD6NaM6kxVDqXsbc4O6lxZeBfTEYMciOqlWykNXzYamBbnNkkQ8hM13cWMhgl
n5ME+pwJuAaoXm//3eF1gQ==
-----END PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFLTBXBgkqhkiG9w0BBQ0wSjApBgkqhkiG9w0BBQwwHAQIOqrbtx8l33ICAggA
MAwGCCqGSIb3DQIJBQAwHQYJYIZIAWUDBAEqBBD/10M+FtmDkUQUz4FsBByJBIIE
0G2HxTt27fvw5RabI0ymtNuwMK5s7h30echw2zQCmzGB77dx4ykvV409p8/liWMr
Wzyf7nyn/4FRKuOzHEksfelBxi8mQ82SwQtPjvGMaN8+PnbdzpPXHgbKQv3BhC8t
1tLt/qkeI8u469UoG8EI9zoRmWISHnYBMWLJi7nUQKsDIo4ADeEMjTNIZIWriNlv
frK4u1ezL7m+mWLqak7VGg8oDaPdwumyXwhIwsdf8+5SewYMeMtHzBBAwZX051v3
n4lUjcx9ZhQM0z8IK8h0nn+8dngxId/EqhahdQJSEOI1gXdMvFy8+jCxtjV9TC1o
M9CDzWBkS/xrXnZEP9zVrO6XGPXI+ocnCoWPZ8VKgp9Zgk87T1oQCSus5eA09PNH
aVcvwnFzVN7lTsdjQD190uZK7Ub5E6XM8dAihar4wK0ODIIDf7PXhpoLTP4Dm94J
zjZu7HnVPjycjAXWeYsgUechX81df1oyXyE9wFReO+jGFFI/NRcG9yLefYOsVclN
tPUxTHjUVxXDZvjzPJanXpv0jef/IEt1UW39Rt/IdzLPXwglLptDAQwwdDaH0PCT
4pG08KKYKtw/xvsEUPyGk/8DR3R03Ae0M/K2bmZExJtWlkCYhqaT/DqFD7zmmmEv
roAnBESxuJDv77DIrK3zQ/FfMxe8LosUuYAEWF5f65xBv8y9o1o0H9KaofhplrYD
GDOJkwUe5T+9hprkZ5z3GfAR8eA6B8ANZN3fEI/WMAFBvPDLJl0ljsc7yuwgLj6q
IrM8xDXizrvldG5gJTGGFdYh/5YNRWp3sWBxwMpydw21wcWbyeXBo/kh8v4henRU
GBYjh+S59alh2U/Vz+M3diZYphxy6VDzhmSE/PVZX5cNgv9gx/fn+Owhm9tXvrwT
z8NwTiCeoSnxY5UJuAQPOTQnzIkTQUvNXdTO2lM+hmzn6mgx54QSw9BtBCpcoiwF
4jzBP27T8qpp63wSiexmuRll//s1ZAc0rQjVh64QRJDWP021AtdkTZTmmp6SxP92
grqHVaJU5MbwkR8byEjwYjw0Yn1iayOQjC1fiqFlOAgBY4vlnBMP/0iQIponqXNz
UFx1yMOljW1QDfTNGfmTdeKGuVhYRR0Y4kcaO3bbCIF4+iM5nkcUyywd7vfcW3tD
qXPojuNJqmnbYdyWfksdZfESU88jHrMV3KLm6KpjvJp8OomTnNxi5ZzCIkg4nMrB
dXof62wx8NcUpC9iSHoWXR4igwZrQX/F5Mfq+MNLC4MXoYputXutmKuVe7zpqMy/
NKlxagAonBlJZYUvrTH8FnTjgQSnPvF7xxVxQIB2Aexa/iAWlsISHzLBO6s92VOG
5EHXJ8GfE6w+A50TaAAhd4X/lEhOHKfSh4sZwoxYPR7apXrarlUMt18G1SGGxt8e
aZeDwKQm62bChHNzV1F9/DWJId3P+XrIfWlCsZ3D2s+7Kin5cYdLJLOeYOgY6juQ
1hkEyuTiUhhKmD4q9R/9xeobj7hH40hsQpzE26e135R6ziUd9NPBTZ+WN//fmNmz
kUFVEGm76EV1DRFL8fx5nzGOn5j6BFkNAwyB3cgjUeIMi21YF5FuFVQ63kJ3oOYx
B5wdnq2QDNYAi9IG1QMOuybhZ4Exf3LgTLvrzMhH7nbi
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIEDDCCAvSgAwIBAgIUW0kY9QaXge3lwVXlvmkjJdRB99owDQYJKoZIhvcNAQEL
BQAwgYUxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5
MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxODA2BgNVBAMML3JzYTIw
NDgtY2hhaW4tZW5jLXBrZXktaW50ZXJtZWRpYXRlLWNlcnRpZmljYXRlMB4XDTI0
MDgyMzE2MzUzM1oXDTMyMTEwOTE2MzUzM1owfTELMAkGA1UEBhMCQ0MxDTALBgNV
BAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQL
DAVJbmRpZTEwMC4GA1UEAwwncnNhMjA0OC1jaGFpbi1lbmMtcGtleS1sZWFmLWNl
cnRpZmljYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuFT3PFZ9
mfnhnoKHqsiuSO1mJdZgbn6cYp1voZI5MXatM8W1BqejEr8hsKDMSEw8pPJnobn1
CmmnT3QOrL16hMDDMpuQ61lwPAag+owX43byD4WtXlypXlg5BNZx6LAhDHGexq48
a2zkrUSmxn7MzkFoCKRCVp/I0rAS/sPNFWXFIUPPympSv+9Av6rjE95/dZfiabrr
KyvYnnkA5TQdgSiKZ/PqP6NRuy1hNLIGg9lbKl/2ngcYP/uej9MJ1gDxePv+vCz9
XOF5NZOjYwxkQGL11ZZFjV4hc2d/C0LUnHqjLo9CXdxsrlA4mGqWPXNwM9x6dFVj
ponkjscVTL4DSwIDAQABo3sweTA3BgNVHREEMDAughYqLnRlc3Qub25sb2NhbGhv
c3QuY29tghR0ZXN0Lm9ubG9jYWxob3N0LmNvbTAdBgNVHQ4EFgQUpZ3jhDJhSHh1
0dJWCGbNTorHJGowHwYDVR0jBBgwFoAU+eaKXANc0bK9hQyo9+1qtJu5W5MwDQYJ
KoZIhvcNAQELBQADggEBAL2yTvDDaGE03X3eDAV08QnE+IUSSz8aV5XT7R4sctMH
Vy/ujYYY7EzhE5YEZxEapkoMj9uV8BugIyEDo3NpcVVKsWKRa3EXYg/ELhz0KBqT
JvqDtBXWQkDaL2vHj6pltv5DRpfhnv6BBIv+83Pk1CE8/R22t3NqhQq4c6HDQwlr
x+8/DwaRGtsLi1ku1iURu3rgrmeMycxccba3SSMgv9H20xhB6DNxw3arRSzWu8cU
aBOKKTlobDbI2dvO1+OaG593Pyk3ttQqD98iVsxlWoyTM4nnPyZCpFJYxqYB5aal
x839keqWMmMpHoaHuYyOFR2AqDru5bKgx9tJ8IazFsI=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIEJjCCAw6gAwIBAgIUZhZE8teYmtPWRkOR5FaFWPAFhNEwDQYJKoZIhvcNAQEL
BQAwgYAxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5
MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxMzAxBgNVBAMMKnJzYTIw
NDgtY2hhaW4tZW5jLXBrZXktY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMx
NjM1MTBaFw0zMjExMDkxNjM1MTBaMIGFMQswCQYDVQQGEwJDQzENMAsGA1UECAwE
Q2l0eTENMAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUlu
ZGllMTgwNgYDVQQDDC9yc2EyMDQ4LWNoYWluLWVuYy1wa2V5LWludGVybWVkaWF0
ZS1jZXJ0aWZpY2F0ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANzS
IVV5aaWiUb3fmG90NBS1RsC3RK4w/JC6dbs92L//alFwkkww4OolS0tUyeRAHvir
bbgmFoFVEGKifioLe2T1I+4gMRj+EOTyp/FAAWqNfIWuUcjBB9yuU1Epvy4AaYIE
kfaBGdMBLm7yrUgg4M+f+8i35Wrk/LCM18/NhxXrNjgxf+kQRDXzIHMdsKlbWHoR
69cnvKT6W28gySq9Cn6vKIFA/JDOGhf/2AhK1OR3aU/V9fAPiXHK8a/c9Q2T40yl
oAfMj+bnoIAQhl2REPU101YxvDtQCP3r6n0XwE57+TPLcgcy9YxQjNlOIFkh5FCC
449dy/O0qehHXRuzPFMCAwEAAaOBkDCBjTASBgNVHRMBAf8ECDAGAQH/AgEKMDcG
A1UdEQQwMC6CFioudGVzdC5vbmxvY2FsaG9zdC5jb22CFHRlc3Qub25sb2NhbGhv
c3QuY29tMB0GA1UdDgQWBBT55opcA1zRsr2FDKj37Wq0m7lbkzAfBgNVHSMEGDAW
gBRRc1rxiXEpO/ozCbYXEBuMRQf5cjANBgkqhkiG9w0BAQsFAAOCAQEAFns3eoWp
UN3G6lFk9Uj/+LMGZAF9lPZOVsJ9duVWevw2LvkJJa460/teQ0zlshPRttfuVTyg
JGCuiXBuRasYFTE3BOGrer4h/2hPNMhNsvXYvGVBsAJdxsXJcraxLRbxC3CzMaBE
X0ByPTaozHC6kv2lOy2u8TKz7S4dqWvZEzhQaH0g3yQ+EcoOhkN9e+RAtmsgPH+2
sp1SRqnUFHI7HqRSb49WmBpeJHU3SsIEyzy+sR1RvEdqqlKS38BodWk1oWt38Flv
DLVGOFPHzy7+kB21az4NBjmv8CX1PVDDyvlLKh6Wf12zPmL7oo8cZI/ZSl/fPYW6
YtLQvP7K1PhTvA==
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
 Root CA key: openssl ecparam -name prime256v1 -genkey -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Certificate key: openssl ecparam -name prime256v1 -genkey -out cert.key
 Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha512
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICNjCCAdygAwIBAgIUBDyrXiwlAMM7eEBmYJttWXR2LiYwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjM4MjNaFw0zNDA3MDIxNjM4MjNa
MG8xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxIjAgBgNVBAMMGWVjZHNhLWNh
LnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARhC8KD
3sbAiQkVdOUEJXcPAuTJSvnjj03kzn7PSUrFdli0aHgwap8kXjLjCB/0BRlbZDIc
OFUXCy7tKP6/TPVZo1YwVDAdBgNVHQ4EFgQUWwY2rU9/Bz3YoHwsEWCbm/Fkfj0w
HwYDVR0jBBgwFoAUWwY2rU9/Bz3YoHwsEWCbm/Fkfj0wEgYDVR0TAQH/BAgwBgEB
/wIBCjAKBggqhkjOPQQDBANIADBFAiEAxVvf5TeD4EMmUOuJ8MpHM7ewwKmv/1gs
DwpG2c9k9uUCICnXhdEoRp9y6ZMoZyhf87P/qg9U2jVQiIh2w4lc8oc3
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIMeQ2he5fgIpb9CnoJu26OpXQ4KC/Un+Q1zZfZpDmgv5oAoGCCqGSM49
AwEHoUQDQgAEiWBxFT/O5SmCxTyW6oiz6bsPi36tRObwzY+SRQWjQErFyuQAHyvm
MIM1/1gTZS20QOHMKs8CrlWicNnT49S/dw==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICUzCCAfmgAwIBAgIUaZeKozE9PMTyEyFnRPlOWi/AaMEwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjM4NDdaFw0zMjExMDkxNjM4NDda
MGcxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxGjAYBgNVBAMMEWVjZHNhLWNl
cnRpZmljYXRlMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEiWBxFT/O5SmCxTyW
6oiz6bsPi36tRObwzY+SRQWjQErFyuQAHyvmMIM1/1gTZS20QOHMKs8CrlWicNnT
49S/d6N7MHkwNwYDVR0RBDAwLoIWKi50ZXN0Lm9ubG9jYWxob3N0LmNvbYIUdGVz
dC5vbmxvY2FsaG9zdC5jb20wHQYDVR0OBBYEFE8GtLgzUl2EIYSTkNIG80IHy0JQ
MB8GA1UdIwQYMBaAFFsGNq1Pfwc92KB8LBFgm5vxZH49MAoGCCqGSM49BAMEA0gA
MEUCIQC+1nIa93mMvzL3H3Sz+z2erpH24UELDyAKOm6YD8R4BwIgKj87gyXrhzQw
qKXY3vMpi7GtimhgpcicD/i3pGHl8+Q=
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo ecDsaEncryptedPrivateKey()
{
/*
 As ECDSA, but encrypting private key with password password,
 using the following command: openssl pkcs8 -topk8 -in ec.key -out ec-encrypted.key

 Root CA key: openssl ecparam -name prime256v1 -genkey -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-enc-pkey-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Certificate key: openssl ecparam -name prime256v1 -genkey -out cert.key
 Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-enc-pkey-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha512
 Encrypt Private Key: openssl pkcs8 -topk8 -in cert.key -out cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICSDCCAe6gAwIBAgIUDjQgZ9MfbDRSVe+KFPxuqq5r/J4wCgYIKoZIzj0EAwQw
eDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTErMCkGA1UEAwwiZWNkc2EtZW5j
LXBrZXktY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjQwNTRaFw0zNDA3
MDIxNjQwNTRaMHgxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQH
DARDaXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxKzApBgNVBAMM
ImVjZHNhLWVuYy1wa2V5LWNhLnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIB
BggqhkjOPQMBBwNCAATfISFPBnD82YqIN+7ozDbe9En8eLXb3EvjZ9jgSBXB8NOM
tb4rItHlmFj9UV8twNZMihzszxKd4luEak4oBNpso1YwVDAdBgNVHQ4EFgQUN64d
+3pNQzIc9/Utxlx1NQWwOzAwHwYDVR0jBBgwFoAUN64d+3pNQzIc9/Utxlx1NQWw
OzAwEgYDVR0TAQH/BAgwBgEB/wIBCjAKBggqhkjOPQQDBANIADBFAiAM6Jrbs4Db
5XP3JC0mS1xCqeCXyDeq9xUQ/epi+FgiWwIhAJ7pV03d71j4u14EBGQA3xZNGuKG
pszLD+Vy7mb3y5K8
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIJPXpr+kxk5056kLfACbRvYZ2wEonStzOkSn4wdkUssUoAoGCCqGSM49
AwEHoUQDQgAEsJEe6e3H+BkR/wnZ1VbHmlItLyAQvXa+moX024LicMSrvrx6oGJB
ZliKE1myoWExEFCK43NoaVcDT2j2asNLzg==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIHsMFcGCSqGSIb3DQEFDTBKMCkGCSqGSIb3DQEFDDAcBAiib89tHLNyBwICCAAw
DAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEENgAwp/2yDl6xU6jeUuxRj0EgZA9
Q2Ues8/IxWCDUsX0YZs5ZibhpkgM7WtWWvXmCz9/PGO5LD+CUWli4gIBmgmz8vPr
o5CmshIWRo7dA06ayezDSHn1eIMPbq0S6ZIWvBHyeIniTGHK5Wk9kWDth11zDsOf
W5y1vhyRWq7DrZQR0ClIFuKdAMBAZonnFu9aqMDstq5zPUJwX/KD0Qx9l7/axos=
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICZTCCAgugAwIBAgIUTqtvPTWCA8QesMG5UwdJjstkHLgwCgYIKoZIzj0EAwQw
eDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTErMCkGA1UEAwwiZWNkc2EtZW5j
LXBrZXktY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjQxNDBaFw0zMjEx
MDkxNjQxNDBaMHAxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQH
DARDaXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxIzAhBgNVBAMM
GmVjZHNhLWVuYy1wa2V5LWNlcnRpZmljYXRlMFkwEwYHKoZIzj0CAQYIKoZIzj0D
AQcDQgAEsJEe6e3H+BkR/wnZ1VbHmlItLyAQvXa+moX024LicMSrvrx6oGJBZliK
E1myoWExEFCK43NoaVcDT2j2asNLzqN7MHkwNwYDVR0RBDAwLoIWKi50ZXN0Lm9u
bG9jYWxob3N0LmNvbYIUdGVzdC5vbmxvY2FsaG9zdC5jb20wHQYDVR0OBBYEFKul
9mP/BZr2KoBYcUDbvm71dVJ2MB8GA1UdIwQYMBaAFDeuHft6TUMyHPf1LcZcdTUF
sDswMAoGCCqGSM49BAMEA0gAMEUCIFtfPCUOZN2bHX+yBqpC+/+9VMqlZk2mqO2M
c9KCr8DHAiEApn1uUi2xeFWIW5xfJJwBTaBXNklqsr4WIX5WTePnt2E=
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo ecDsaChain()
{
/*
 EC Chain OpenSSL Commands
 ========================================================
 Root CA key: openssl ecparam -name prime256v1 -genkey -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Intermediate Certificate key: openssl ecparam -name prime256v1 -genkey -out intermediate-cert.key
 Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-intermediate-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Sign Intermediate Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -days 3000 -sha256 -copy_extensions=copyall
 Certificate key: openssl ecparam -name prime256v1 -genkey -out leaf-cert.key
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-leaf-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICQTCCAeigAwIBAgIUY2J369z13OHirAedRguqrhBhzwEwCgYIKoZIzj0EAwQw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjQ0MDNaFw0zNDA3MDIx
NjQ0MDNaMHUxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxKDAmBgNVBAMMH2Vj
ZHNhLWNoYWluLWNhLnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAATNbJNz5cT/CMwO5vsa7LJE7p0GZ3T3Y2C3B9Nfy8gK8vM/Imjqs31+
F7JTZWnO5Cy0IOirWdMOt5Wnruq6QEFro1YwVDAdBgNVHQ4EFgQUDKRA+3NTzvdi
gFWAth8lt61TWkMwHwYDVR0jBBgwFoAUDKRA+3NTzvdigFWAth8lt61TWkMwEgYD
VR0TAQH/BAgwBgEB/wIBCjAKBggqhkjOPQQDBANHADBEAiB6BxETPz3mQpgx3hoe
/uo9uyjQn07M7jeNl92kBqpWJAIgV2bykRM+pmmOxLNqatb0wEjC1zCSuQT02NE4
rVwLyq8=
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIOac2orm4ZK/b/AX01dh2eSSWHU1i0zFlRY68WliNHjFoAoGCCqGSM49
AwEHoUQDQgAEFA93xTXJYZD4olJ4wyJWYWX8VqMTcIy/QmdxHZ29iz/ejP5+HF7t
luQPLFoAF4b6S4ghh3r6rOZDNAOMqFh+eA==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICajCCAg+gAwIBAgIUKMXi00BZ3nG6Viw5J70lv+9ZNyswCgYIKoZIzj0EAwIw
ejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkZWNkc2EtY2hh
aW4taW50ZXJtZWRpYXRlLWNlcnRpZmljYXRlMB4XDTI0MDgyMzE2NDQ1MloXDTMy
MTEwOTE2NDQ1MlowcjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTElMCMGA1UE
AwwcZWNkc2EtY2hhaW4tbGVhZi1jZXJ0aWZpY2F0ZTBZMBMGByqGSM49AgEGCCqG
SM49AwEHA0IABBQPd8U1yWGQ+KJSeMMiVmFl/FajE3CMv0JncR2dvYs/3oz+fhxe
7ZbkDyxaABeG+kuIIYd6+qzmQzQDjKhYfnijezB5MDcGA1UdEQQwMC6CFioudGVz
dC5vbmxvY2FsaG9zdC5jb22CFHRlc3Qub25sb2NhbGhvc3QuY29tMB0GA1UdDgQW
BBT74hc+vRwH80LdeS6hfR5yJO9PxTAfBgNVHSMEGDAWgBTg03X64zrgvOeRsBMc
rlXfXsBifDAKBggqhkjOPQQDAgNJADBGAiEAyr8SivAp0yu5k4gkjUqTzTAiKFkG
TNzKHWEJn0NvZcoCIQDvPkYUK/XMA6AA8dR/LzF8ALGeqk/IondC0HUCj0CtYQ==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICgTCCAiigAwIBAgIUTyNdbeRg66vQWvC4f1x3iLwvQ60wCgYIKoZIzj0EAwIw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjQ0MzBaFw0zMjExMDkx
NjQ0MzBaMHoxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxLTArBgNVBAMMJGVj
ZHNhLWNoYWluLWludGVybWVkaWF0ZS1jZXJ0aWZpY2F0ZTBZMBMGByqGSM49AgEG
CCqGSM49AwEHA0IABKo8QKM8ku5uZhEa3BsLP9acAJARcYnc3RhLnjhFPpBMKNll
jNwKXc8/OEYvFsJ3Xm9Il4idh32MBnM9CfURXyCjgZAwgY0wEgYDVR0TAQH/BAgw
BgEB/wIBCjA3BgNVHREEMDAughYqLnRlc3Qub25sb2NhbGhvc3QuY29tghR0ZXN0
Lm9ubG9jYWxob3N0LmNvbTAdBgNVHQ4EFgQU4NN1+uM64LznkbATHK5V317AYnww
HwYDVR0jBBgwFoAUDKRA+3NTzvdigFWAth8lt61TWkMwCgYIKoZIzj0EAwIDRwAw
RAIgSX7qsTemrtM5jJzmNPxIlGotW60US3JUccvqPEh2FsgCIC2sZIyMt4EvygEU
Ly81qCozaOirSkdmD6ANPStDH9GD
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo ecDsaChainEncryptedPrivateKey()
{
 /*
 As ECDSA chain, but encrypting private key with password password,
 using the following command: openssl pkcs8 -topk8 -in ec-chain.key -out ec-chain-encrypted.key

 Root CA key: openssl ecparam -name prime256v1 -genkey -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-enc-pkey-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Intermediate Certificate key: openssl ecparam -name prime256v1 -genkey -out intermediate-cert.key
 Intermediate Certificate sign request: openssl req -new -key intermediate-cert.key -out intermediate-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-enc-pkey-intermediate-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Sign Intermediate Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in intermediate-cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out intermediate-cert.crt -days 3000 -sha256 -copy_extensions=copyall
 Certificate key: openssl ecparam -name prime256v1 -genkey -out leaf-cert.key
 Certificate sign request: openssl req -new -key leaf-cert.key -out leaf-cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-chain-enc-pkey-leaf-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.onlocalhost.com,DNS:test.onlocalhost.com") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in leaf-cert.key -out leaf-cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICVDCCAfqgAwIBAgIUARNdnj595mvUQn2O98LbMUjVrCIwCgYIKoZIzj0EAwQw
fjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTExMC8GA1UEAwwoZWNkc2EtY2hh
aW4tZW5jLXBrZXktY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjQ3MDZa
Fw0zNDA3MDIxNjQ3MDZaMH4xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0w
CwYDVQQHDARDaXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxMTAv
BgNVBAMMKGVjZHNhLWNoYWluLWVuYy1wa2V5LWNhLnJvb3QtY2VydGlmaWNhdGUw
WTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASRTIKW24lUG4n6Vehcrbe6frNFRNtc
Pil8CEMdSvpaguj2KQsi5iYNbXen+SeA6tCiHrvPWslD6RP/w2oDs8j9o1YwVDAd
BgNVHQ4EFgQUgKa3W11+oC9J8icLoMPhR4KXOcwwHwYDVR0jBBgwFoAUgKa3W11+
oC9J8icLoMPhR4KXOcwwEgYDVR0TAQH/BAgwBgEB/wIBCjAKBggqhkjOPQQDBANI
ADBFAiA3gD2NT3Gv5N4e8LxtnCmSeFPcSb9F9mFohb4Cu5w9+QIhAPEM4VJVhnGQ
yV9i62s8JEr9Vjh+ro1uYgPwTisaPcr6
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIKX6jRwOK+c7s1QqFllspWv9QgmpsL+ONwJuJ0/m5B9+oAoGCCqGSM49
AwEHoUQDQgAETSb6tx6NtMVkLQ1K7SMTfjbiyKDWjR5tVQkCWqg4/W1x2KBe0nbu
z4wkWe2rsgDCH9P3G7OuTaK1QBs/b7yZmA==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIHsMFcGCSqGSIb3DQEFDTBKMCkGCSqGSIb3DQEFDDAcBAh62Z2tTDYxuAICCAAw
DAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEENEeeYP75uF4kYZztwMdaAcEgZD8
klPq2pusydN3JVNxu1th+ORAybEuy0oZaXipJr+otycHobrMjuKIGJ54W/o1rQ3K
CC0rQFHiQN1dhVlGm3FEIwTwcywoQmja7PPUo73q6EkdaI6ADO3tEl4kX+RzpJ58
C2xBH7MujeSit3KaYNneNC8LQYfHYnCEsdcgXThlmLb/XrlwaAqGJT0zblQUuxo=
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICfDCCAiKgAwIBAgIUILHh18naAPaJgToND7Av6+MttAUwCgYIKoZIzj0EAwIw
gYMxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxNjA0BgNVBAMMLWVjZHNhLWNo
YWluLWVuYy1wa2V5LWludGVybWVkaWF0ZS1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMx
NjQ3NTlaFw0zMjExMDkxNjQ3NTlaMHsxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARD
aXR5MQ0wCwYDVQQHDARDaXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5k
aWUxLjAsBgNVBAMMJWVjZHNhLWNoYWluLWVuYy1wa2V5LWxlYWYtY2VydGlmaWNh
dGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARNJvq3Ho20xWQtDUrtIxN+NuLI
oNaNHm1VCQJaqDj9bXHYoF7Sdu7PjCRZ7auyAMIf0/cbs65NorVAGz9vvJmYo3sw
eTA3BgNVHREEMDAughYqLnRlc3Qub25sb2NhbGhvc3QuY29tghR0ZXN0Lm9ubG9j
YWxob3N0LmNvbTAdBgNVHQ4EFgQU64tqArj1swBD/QUfeVLjiPljM0EwHwYDVR0j
BBgwFoAUQi8DuhjfUbJCN18LwMEpumhCqEIwCgYIKoZIzj0EAwIDSAAwRQIgAmXF
Pb8rb5XqHVFbzfxR10Bj6O9tOsG1EjhQvNpVHwACIQC3UliSo1C98mPawsHt0+SX
eXHy4sNknm5T3ISY13FeFg==
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIClTCCAjugAwIBAgIUfNK+kB3pXMW4jB/lUQ598c6yVdMwCgYIKoZIzj0EAwIw
fjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTExMC8GA1UEAwwoZWNkc2EtY2hh
aW4tZW5jLXBrZXktY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNDA4MjMxNjQ3MzBa
Fw0zMjExMDkxNjQ3MzBaMIGDMQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTEN
MAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMTYw
NAYDVQQDDC1lY2RzYS1jaGFpbi1lbmMtcGtleS1pbnRlcm1lZGlhdGUtY2VydGlm
aWNhdGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAToO/BgPB8qqTQwHptI/i8d
WO5l0J2fgxJTrOrRrTts87c3jrgtkPF0kmi1dMtd/0F1DhM3MHliSx/5rrm4xFJL
o4GQMIGNMBIGA1UdEwEB/wQIMAYBAf8CAQowNwYDVR0RBDAwLoIWKi50ZXN0Lm9u
bG9jYWxob3N0LmNvbYIUdGVzdC5vbmxvY2FsaG9zdC5jb20wHQYDVR0OBBYEFEIv
A7oY31GyQjdfC8DBKbpoQqhCMB8GA1UdIwQYMBaAFICmt1tdfqAvSfInC6DD4UeC
lznMMAoGCCqGSM49BAMCA0gAMEUCIQC18FpIVhyLcRI7j3Mu9Nhc4AmEbmNnJwLU
qy20x00YKQIgLZq84lDoVPfDnlyOcnY3ZK9Xwl1NTG5v6OU9/ZMtc54=
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
