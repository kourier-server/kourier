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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgITTGwUQoEvxmQtZtl4fYjNc62NfTANBgkqhkiG9w0BAQsF
ADBxMQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwEQ2l0eTEO
MAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMSQwIgYDVQQDDBtyc2EyMDQ4
LWNhLnJvb3QtY2VydGlmaWNhdGUwHhcNMjUxMjIxMTgzMTIzWhcNMzUxMDMwMTgz
MTIzWjBxMQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwEQ2l0
eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMSQwIgYDVQQDDBtyc2Ey
MDQ4LWNhLnJvb3QtY2VydGlmaWNhdGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw
ggEKAoIBAQDnhwAPTR7fYVx6Gnu33wjDhz6PG3z+TV+jqNaBFqsiNPtQ5MlUcMXM
WRXLnCG88CfoGiKUf0tAJPBZrdW5fFDcz8t1yiH8uPf5buk3JiTtNOCAMMVQdM7w
TlvKmKSfkZwq6llhRg8C9TEFw9HBY6Ln7ECa6dhzgot9LDwZ9X8wM9h0jpwoc1sK
Jm3bzlC9UczM+JKeK/X80iLHbO8YGro1pnz82+jKA1aOr9zgs5SuqiMBJDuN3YH9
qPGuzS67m7requPkS2D0HKqTofkXHiBxVkaBEu4Ujrz2LubgurL+ukpR02CQJ2Da
NgIGwEj20RdrRYcaLpUnmCR0EQes3JPHAgMBAAGjVjBUMB0GA1UdDgQWBBTyFZFJ
syZofjbin8yD1peMtzLnxDAfBgNVHSMEGDAWgBTyFZFJsyZofjbin8yD1peMtzLn
xDASBgNVHRMBAf8ECDAGAQH/AgEKMA0GCSqGSIb3DQEBCwUAA4IBAQAz2Pxnak8K
WmHce6NugdFVzFs1kqiasXNLlylaDvlYWH6NhQwDt6VAsFm3fiOBMkyOTHeH/yB1
+SmVfgnWmnc5SVTYfcpUSO+o3qsl+rEdVmuyem1ezRX7xHiLeZwCnI1Q5SieuwSf
IwqRsuwocd6iMOd7b/WL7ag1Zj851QZ6H0HH0WXyS2zpRWGKJmSQrLsTDDG+pRA4
yN0ry2o36mNjj2RWSVuqLiPXvy5VmxyRszGe+hsIAzvgl2qEYJTGcbgEUUk7uZhD
2X0/ce30LMucD3VrLUBD8FGWZBMj5nvbRys7M//LmLqj9wk3Be53LRVyFGOSpWwc
JWsJScoeXIPf
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCO7FTyohKklWAm
OT+zJwmsU98Legy9bqZCeBlmAwctremN6ccWJzI0GtnbobEFaXoBZUbZTIKOjIBl
kyWZZWcPdhQH0zaouasqIR/wwTgtmpl7c7jGEGVWxemXiVBSUAuC1MuV9d6/NBTU
F2QWYt971yw1lQhW6MnvFiWNv5+HuGOuxIRu7Lo8cr7CcllaV2G65OJKP1RI7Ejq
VIAtNrHZSmmS5fxayFWG3rRKvVvzsx9kGw3WJ4aBrG5B2HzW+6gGvuv9AtIHzf7r
RPitVRNqetGeb/JCQxRcZCFU4BzyFm94TKG90P+v3rczLYURNgdEj6+LfDwxkuOj
7lDqWMONAgMBAAECggEAB9P25SEuXzhVP1K0Y/YX6Anh9gl1yCxWy94zGezBiGV1
nNG4l0SHUeZEl2x4OyEnTwtCgaVYo0EbyTRxg0DUD7s6ZqFRZZVhxBlzW+bhUlIc
5O+WJ7b/Le0CGcwqC5AZyU0pZ2nDjPpnx6QOVdGNhwVmhPrILaZrZP50CRr9PFwu
mUSajuaTAlfF+5/hdqwiM3ExxGOy5jZ7m7iTuvH6+jO8NgLOJ1cp6+K74Ym311Ku
V1ZmIi3YzK6DWw//20XvfWQMVThrzJ6rmJ91Vb9z+4AulgsfB1CbZzM+f8VojFdF
xqcRKfG9GMfMOSBjiYShq0VV/ZEd3PxpLPWZbbzcewKBgQDAzbd2U5FJuPr9xG0/
eZQ/E2+dE35tzuuu6Nt8A+FFQ/cRcsZc7H6s72he1fpq+YzXwacx8/koT7SHzR6R
mYe+LcdgxfP5SOK8Q7Y0pTx6+uhNgrg4rAk12b0dHoz48JOH6QLAinARYhCPyuHJ
8wtqdugZySV2fi0pASnUyxOy7wKBgQC9xR14jONF8SLupM82lIPiY+QDmCRn0fJ+
P4HT4HdXh/lGPhOO39c+dxuh7S1AR/UFst/fsYr6NCguO7eJFeKwYG9YD2LuNb0j
HE2eZWWISQxbzAD27QMCXKoayJtdOMLDMTrYLZK1bPGQSZ7gmkkNbFbeZ5Pl0p2Y
lxZU32IBQwKBgFEDn/12a+xAcCbFrOopGiPO9O1ZzZwygFIEwoU/1B6Yym5offV5
4likWITRAOKGbaoTUz/oNl+77i3KAPhJPQKqlcaSJ0wuViephBxm+hzq5UX8kZAu
qcgY8CAWQEIyRKutO1zQTvre6/qf03RaTge0Yx7CqJ07s5oxRPSHSIW9AoGAQi3m
vC6t56tTWlQha1BZue7KCrLZJ1ehMIPnO8OQ+vaRaPynB3Fqd/9DpLG40S0G9ujN
iA47gEwGIzFyLR1zw7ytt9EN2DLndcIeE3oa8W9AHxdC0toO7ZjxPYvuGpF74mRd
uBwUiK6og4AZJwzHIyZAGSmX+1L64Mb347tPS08CgYEAsvU/CZFqJetaBJI/OdmA
Dw9VKEzQC7M17+7rZ+0T+0tXEXCj7CDOrtOkOcJPxb64GiiwjCfpWmnaXtd29mGr
5Zlvcr39qpm9xFFBl5PtIqQLA38FqTfG5LToZhOApRBUmU5aZKgYm5S9wyCViohK
DcrdlO48sFJGJDQ5MyxCdjE=
-----END PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIEMjCCAxqgAwIBAgIUKuRJ2EaaNp6yaHQiIEkWKSISxRYwDQYJKoZIhvcNAQEL
BQAwcTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEkMCIGA1UEAwwbcnNhMjA0
OC1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIyMTE4NTIxNFoXDTM0MDMwOTE4
NTIxNFowaTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENp
dHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEcMBoGA1UEAwwTcnNh
MjA0OC1jZXJ0aWZpY2F0ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB
AI7sVPKiEqSVYCY5P7MnCaxT3wt6DL1upkJ4GWYDBy2t6Y3pxxYnMjQa2duhsQVp
egFlRtlMgo6MgGWTJZllZw92FAfTNqi5qyohH/DBOC2amXtzuMYQZVbF6ZeJUFJQ
C4LUy5X13r80FNQXZBZi33vXLDWVCFboye8WJY2/n4e4Y67EhG7sujxyvsJyWVpX
Ybrk4ko/VEjsSOpUgC02sdlKaZLl/FrIVYbetEq9W/OzH2QbDdYnhoGsbkHYfNb7
qAa+6/0C0gfN/utE+K1VE2p60Z5v8kJDFFxkIVTgHPIWb3hMob3Q/6/etzMthRE2
B0SPr4t8PDGS46PuUOpYw40CAwEAAaOByTCBxjCBgwYDVR0RBHwweoIMKi50ZXN0
LmxvY2FshwR/ChQyhwR/ChQ8hwR/ChRGhwR/ChRQhwR/ChRahxAAAAAAAAAAAAAA
AAAAAAABhwR/f38KhwR/f38UhwR/f38ehwR/f38ohwR/f38yhwR/f388hwR/f39G
hwR/f39QhwR/f39ahwR/f39kMB0GA1UdDgQWBBR/Z4aZ0mvxvGTuCpSnhZ/TG0DW
/TAfBgNVHSMEGDAWgBTyFZFJsyZofjbin8yD1peMtzLnxDANBgkqhkiG9w0BAQsF
AAOCAQEAgewgc7gI2OgSlaIMbb2lcr7qFmY9ghcuh3EuKhA/dp6AfaWE4SEYAmr2
AWSuZW2qqdxo7AfNk6R7N8dC3EpcXM9Ydz49aED9I+fWOvz32qLy9ROCOI9H50Vw
VMlEsO7kp2JA/4alzqraoXk53HQ63w7T634Ft98UEFJ/2blvbUONAr6K0Eu4LK8D
I0oZa9RzhxwUAOh8XDwtd1PnjsNTzfHQs6pzhQd5EYmBznt6CxQ0KomqnJp+DkHQ
RO7Gp1lJs7lMv/FraDcUJ9LwkCrhd9LWMef6LQoJU6Z2IhSpqrglNQxidxp9yj5w
hPLehEXe7CEAvXr/oJi5U0TC82N5Bw==
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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID0jCCArqgAwIBAgIUawhebAx3CEBtlg3SxweuStxsd+cwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIyMTE5MDMzM1oXDTM1
MTAzMDE5MDMzM1owdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UE
AwwhcnNhMjA0OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyMAXp+MgX1QJ0UN8oh/F/EZTFlkaWhssm5kT
/N9ca+6s4/lmVt78h6n8wTFMgxD5retl/6Q/Yzl9GwC5o3Nzh7QnhMghH2pvRNFa
q/RFEb7ZBiI/yJOPR7eUIy/GGjDNosEeIyP/2YYg117Tj00BEdmdMdB1jYGRn7F+
7GOzWL516/i111tUcIZd+5bQS8rtCps4TOLWCpJLltR59R/4KWZX+cNAQ5I8JOUl
9NLOQUr2MZXk0wtLJaffk298IQdWIAxwkhfZjMsWksEjMjSBc7IDXMyzuhjhJfS8
Ba+JeNxrmv9VBLUBVaX2CGNbGi4tZF1mqZSKk22T1ao9QjmMFwIDAQABo1YwVDAd
BgNVHQ4EFgQUiuSqcDgfGbgLznnqLW+HX0gtvg4wHwYDVR0jBBgwFoAUiuSqcDgf
GbgLznnqLW+HX0gtvg4wEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG9w0BAQsF
AAOCAQEAZ9PwqDq4kh7IVEJfHnvua36Wcg5udN1cRJy9v+ZYygIs9Hct/K1md2Im
onPYqBlaJbfzVXLeJ71cTgs+fROGyXKTN7PRO8QDI9TwnGCLqPhN7DSPkick2Fyt
l9swvlWfA7LIt964M8YJjbJCUQ2wGeMsVd7uVUTw8Fecuhjk7IGIPFvCxPXIUSxw
16D0fRdKpp7N5DlVtRRFDyEzylxk8GhcGNDSjB9KwG24jTs0w8CFXBA4bWJGR751
Nyd4Ogt4Nn4VBuesBy8xQY/WPw/3ydu7vATYKaVXdu9dC/Mm49YZFz9c6GlFaUl1
rAhW3MP/Cz/oNlsfKK+0PGBZS0Dp+w==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDViNRumcFuje4L
oFplFBONs10SFK0lhLruOjR6xeZhMzwBbOIhgTpnky2OwbJtnf8GTghax/sTyMHQ
sGnVT+6QVzFiYnobYFhk+nxBDII3aNkBa73q3KGMrmsViS9qSfQYND+JHwDOyUbG
Uj2DF/XHf/yCQUIxPpzCc2GtqLcQOTQGMs7e4DFI1rv040z1L72mxb5sxg0bfSDy
CJEe6wBZkUmUwMgfEXwHXQKi0FHzXtkfINPn8kY0by0akhbnkvLh0Ik6sbT95qMw
5aR/JyFneo4820/J096QlA2QPjh/FX8mnzQ+k+r4cVtXNRvNtmwCcP8P8VEMZ0K0
9RISVI63AgMBAAECggEAO+GfxRZcLs8heuQwhVkAdMPgbUIcHkHfFBCBiA54YTi3
cqTgs0mkocgdxgNesKvWiSOX5zFeoTky1P2r6DR+r76r7C22lXfKwiKWdr11xOkj
tejLfRH7QdLl18e9Yr6zc5gvWxo1JUg/jz0f3+P3ukcTfkmjsOCgoE0wDr2Z/LOy
XSWQ3Bc45rCGHr32VAvBgDYjCIZG+z/PPIXpgNFzPKG9eq0dhAlquWyk48wJzPMx
fhynaVIllhs6fgATsliwKBfdJHJ3rDtai5MmYbYf1/UEGqXSFaB3G82Uf5VlTNsm
UA1U03hJZ/YLxSu9dE79vvkNGPFaksAa91DHGBiPbQKBgQD+okLyHGs7NRg+ncAb
QtPcO5Uxl6+1m9gISQyVsNCQ1UvjNragMoxSN3/qLl5tjAGbIVhc0d2p/7/Ituev
s+1A2JYywR0SaVpBkCP87gNE1vSuNfDKFqLCcAKJiddxAULMFLXaTg5AlOaxDIDH
ev9rJ9w5TtJAsSFN2V3dH31jgwKBgQDWrh5YKgqHBRdsi31bjeB5HAU7xoGDjoKp
ysD9uagTO9pWa2AsfZhQAFxEDfClzHFCZxYwwelqqaNlywGcorw0h2nOPemvktPl
yIr3+dNnskhZM+4D6g1ApWqra/uGbly5B2ZcmIk4IxksP9Uac/xdFlrMNChFXPTL
VehW457dvQKBgQCltb9PhMMcIu+GQG82zoTFvFjZj1egHq22pqNz/z+cIdNjKMYs
9vhuwQOTyV1WeVjKNiclMMbKzU9oNfANXlC2dL06hoZ+5uT/6eghwFuMPz+46A2t
1pFRbEeaboQwXJu16Hx4d3e6+6wt6G8eYqM6fxRIj00xIJUF7sGxF78oNQKBgFpA
mUE3a3WYDmDzw5/Z5aUEwq5+pbRJFWWMd9YzVxW+8+ug+K27n20CucdilOkfKf5g
mFOnMhpFdww0bFkNkVIEG9c48CV/9NWFmebmgJn3ubLGwimjHC8xW8b1fqjlfNXM
5pvHBOk02EBiFZUpbc80trditgtzKPbTBvqAdymBAoGAIhAyfuJjGHE+gwxhbuaL
Zw7FmsOgiKappomPLJ5ckqaM1jMwjwZuD17Z8sdrGnj0TRtg0rrVOnST1L/NC8qd
eYihD0BATW5YsNs4FxmI4DFNXkVdyQyX1D568s2tJixVNUghl1kkAZD/UJSsM6fZ
9nr0NZHZPtjrtHUAUMEUhik=
-----END PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIESDCCAzCgAwIBAgIUX3sqt8BsDtVETGMqCVO6Q6BGZGMwDQYJKoZIhvcNAQEL
BQAwfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UEAwwmcnNhMjA0
OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwHhcNMjUxMjIxMTkwNDMw
WhcNMzQwMzA5MTkwNDMwWjB0MQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTEN
MAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMScw
JQYDVQQDDB5yc2EyMDQ4LWNoYWluLWxlYWYtY2VydGlmaWNhdGUwggEiMA0GCSqG
SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDViNRumcFuje4LoFplFBONs10SFK0lhLru
OjR6xeZhMzwBbOIhgTpnky2OwbJtnf8GTghax/sTyMHQsGnVT+6QVzFiYnobYFhk
+nxBDII3aNkBa73q3KGMrmsViS9qSfQYND+JHwDOyUbGUj2DF/XHf/yCQUIxPpzC
c2GtqLcQOTQGMs7e4DFI1rv040z1L72mxb5sxg0bfSDyCJEe6wBZkUmUwMgfEXwH
XQKi0FHzXtkfINPn8kY0by0akhbnkvLh0Ik6sbT95qMw5aR/JyFneo4820/J096Q
lA2QPjh/FX8mnzQ+k+r4cVtXNRvNtmwCcP8P8VEMZ0K09RISVI63AgMBAAGjgckw
gcYwgYMGA1UdEQR8MHqCDCoudGVzdC5sb2NhbIcEfwoUMocEfwoUPIcEfwoURocE
fwoUUIcEfwoUWocQAAAAAAAAAAAAAAAAAAAAAYcEf39/CocEf39/FIcEf39/HocE
f39/KIcEf39/MocEf39/PIcEf39/RocEf39/UIcEf39/WocEf39/ZDAdBgNVHQ4E
FgQUhDzwSRTorPFG7B+qFc3+2nJwdHcwHwYDVR0jBBgwFoAUGNx8cHoNsOFNwv54
MCjTmO8Q5/8wDQYJKoZIhvcNAQELBQADggEBAJRb+aqrBycQq1egh+VAbaYShqV5
Eu86I3LfxI7Ffx4L/LgNd4JUPlXpTHLTOuTXrq6OtzRS0vs9QskWdF5zDDP/qSPN
dJqqCyKTiA4vwUcFaTx+5KAuDyiYPbcO99gJINc8+WMkRQY61gn2N/tb3W0iDLML
M8tVxgq5AhGWhRX2zvhjqMmit1fpSu5pQ9+IW/VIJC+b0bo/4b+fuzJqObc54r9H
0g99Q9fcCg9956g8n4Rk95JnNP5nwj0KGKeXNYmJFsTRbu5yZxShtlOGAsPNaa6R
QwT1xwXexsmLWCbpwAcKodR2o4neG+O99ivssh/b1ejB1h7etSnum266mPY=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIID1zCCAr+gAwIBAgIUHT8a8KvekD0a9AXdbjdjFTgAk3EwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIyMTE5MDQwMVoXDTM0
MDMwOTE5MDQwMVowfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UE
AwwmcnNhMjA0OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwggEiMA0G
CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCWDI4bAeQkUCCnBm59NZEq7aw8HJOO
oK/YX5aO6mnjgkAnuaxRfDlwEPxyjW9Xmf6CzAasYOx1wAROhEx4r6qOkktxZJFH
OHv7DtD3OwLYRVthjhkVmrBMzFg+f+NizPGO6eah347VkME/wPesk+J6VMDfYTgt
lOqhFig9DoPNH2JihqX0xt8yxqUm2PcG18+2gGG+qtgaOTGkQH9UiAbdurOPZC6Q
z5ifKILLpH2OPUFBrWWbHn+Q8iBY0uybbIviU4zsh3sv4JbG9LGOTYwREy5nrEJk
0udnMxx0w2W1s8mRPZ/QPAwdTvMCQeDUq1hOLU/JPn7lEEIo1u5SR+RbAgMBAAGj
VjBUMBIGA1UdEwEB/wQIMAYBAf8CAQowHQYDVR0OBBYEFBjcfHB6DbDhTcL+eDAo
05jvEOf/MB8GA1UdIwQYMBaAFIrkqnA4Hxm4C8556i1vh19ILb4OMA0GCSqGSIb3
DQEBCwUAA4IBAQCstmo5rHUJDhrnzMoGL6MZgl15DU5C2AZWZUSqX4ubaAbFMSF2
3OXIt8J9OCE70D2iyJ16q/kXETLOrIirXY6jsWCTGZNhfaP2mz0STHUFg0vG2hOW
1Xi/OcQLx5j40yhg1/K4MZJqonXNY9MzzrlS0TVH5/eILEvAEt/W2xMTzXvuB0RL
s1skcEKexc5lxLZivxP4SG51svpy9yXt7sZVgLIuXy82rdwt89n1KSbh+8ATrkbl
UednZTbxUbcj13ITvs7kWCXCnpUgqmmcB0Kz+XXtrkLPbZxmcwuma59irnWcgUEV
Andxvsg2zvULkYs41949eqonEzzLgS4PEIoc
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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in cert.key -out cert-encrypted.key

*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID2DCCAsCgAwIBAgIUS1UAsq7ZwTqgSFb5BxYXhmYrb00wDQYJKoZIhvcNAQEL
BQAwejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkcnNhMjA0
OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIyMTE5MDcxNFoX
DTM1MTAzMDE5MDcxNFowejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTAL
BgNVBAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsG
A1UEAwwkcnNhMjA0OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMIIBIjAN
BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAp+xTDbTy7He1pVhT2TEwpzgXww/W
PT2MYJF6fQW8miRndhcMAV+rRhv+tdhYzV6RAyb8wkxfiXCnC6gqK0QfBwJnfAkr
pHg+ArYjaTBWA59yMXQewYJPixYN8KZesmSXuoJ8soKCF7098dkcY9CHSyU72A3N
yUNfpw+AKQC5GQi2AXXddgj7m+zH7NAghkD6AUAoh4E7A1GTlhyN7Sv5bHf7EYMl
G73SvE7XhA1K1iJ0+KEbE27JQQTxoybFPgWRyledDW5NT8Bnx9x6HN24syfGOHpI
Go9EgFGFuoxLLEmqJzsuh5AIAIH4eCEgwPOdKctsvPp3z4OEN8DguVldNQIDAQAB
o1YwVDAdBgNVHQ4EFgQUzwLaYob0486zKUJL7H/R2XkNT/swHwYDVR0jBBgwFoAU
zwLaYob0486zKUJL7H/R2XkNT/swEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG
9w0BAQsFAAOCAQEAMlpKmscHTSaOvaJhbMU34EHN665AXiZLxFwyGbfUpJkr9TdG
CGG1z6BdEcyeycvqUgWsAzakDkSGhxBMdt9Xqvt8wX4SM187FpNluevZYUHEvTTC
PKJiZNRbWT48q6L0UNR3jXS3IEyKGKs6QqBmX5qp/tdXGZf0bHbpv1gqCFe8C18u
5teA3SlHwv6Dnb130Wh5FOR6oL7ke40Hdt8dyqb9XUf54zSlx5nqG5PCeKxaDKZN
26cg1UkjpjrrBHTk/ntDjcybJE3RTMyIH1DEKpMI8xJBYf4MZkFkhPVLbbgWmQAb
jqbzw0iic8IcfC+d2QqBpO6mOyxrl07voclW7w==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDGF0JYZF2khRv6
n5jDSLUec+rIHE+9kO4Cl7PKLmj1ei6ZU4p9SWoVlVSqN4NRqWOFRrZ/eNvu8Ams
xwU1/fzWEriEu1MtwdHx0j1xFE1YjPAaznte5ejo4iv6q5gUTZP8ApGqGZE62Syu
fxSKWI2n5TqkN7aqIzabJ6mgYjhCVpKnZ6n04KFgqTT22U03nPw5EWe5ZQVjiqmg
aXNHUKOmOC61TUHcESVvkzmoUDoAaOlSTLBhxQ5zZv9Y5XY5T45xVtsb8gjLyFuA
789GYVqCKplAoaFMOr4geUvXPHPdrG5xDasBJ5H4X92zjSC6/A8Y/i09iXrjQ7BZ
NxoXgziPAgMBAAECggEAAirt/cP3Zwin3ORoimEzeNusNY1jGAAhLU5rWBU4Q9DB
osmQaeeRmB6JYyV2s+rmcmZq3SvoPY315SShEM1bOh0mqhksrgMBdk571U7Iu+Uq
7s1/Tx4KoEXH/8G8nnhCMXn2YxtNhLcsryon1425hcf943R9ShEBlM1oefSpM0dJ
9lau2QoGkHThPHzN1F7q6d9zNF4Btj+dpM5nOvlCkU0OiKlrSIXJ8FVJ4H52hAj6
ZTSagu9WkHDquPIyIfTPNzPHlmNg3BEpmMwPfNvS4mAKKGXCtzwqTNruuDnyHpE8
dZecFGhis01uTQLbn5Wo8BPQZHLj4r973QzFdbxQ+QKBgQD5OwTBz0WFxi99bXzW
Jlhhf8apJ33jf1R5gl44X53bw3HZQo83x4ivQCCs7x3lb7j493Am/y9LItqPwPoL
hMDgmzPY3OTkIVkUI7T5QUbSbGVLHkB8A1Rt/mPmDwP5XPflx9pMiDHbBVvfWKah
+xMgLGcWjt0N3Ia4YP8m/hgayQKBgQDLeKZORvekgQhA9EGjr0Du7Bao+sYV15ng
uLW5nEOh6ENjaR8IfqS8GyN3rk6WEW7gwvmro7l1k3xBPM4O+MHmpgIf36Lb7MML
4p4HXmk2zjoyq7gk3q/dL0R9WCKzE4bklHjqN1nc5f/Ky4Yv0oyXcZZEOEzCloVN
cxVwoLoMlwKBgQCb5GewVtbUu0d/PBnm1atYqRT5P4OF5mJPctU+i8hw6wpubCyr
Jjr/66vEn/jiR4S67gMP0XSimdV+L6X3yHzgwROoJVtiut0+NKuBcWw8OKMPXofV
SjvoZUPVZjUFKDrDVsPcNSCCMBLsXEY2cZbf/AYrXQc2IWd/MuTGXg9zyQKBgQDA
7YRbQoU70TE/TZsTlnAuW2NdFHNQcJ8yalG9TgL++Rk0oI9RYavXxK/LolZXiu4k
ZhuQeOy4JIEz5nDH120bwxrOmCF3ALcshNMsz4NZ+Q9LznELXK9KPbBmXXSBcil8
gZ9pDdTCVL9GzdZNcAno60X9J5j5BPoeptmUcoIWvwKBgQCnW/voIQWNGWxQdBZa
vKotRLlaAiukDAwytrfK68ulCtRHeYOZ9XgnJRetAI4CAtaTett0cZ03EEVqbA4c
olODzzeXvDz11IB4ULUe6DEvvvRwNCrBUWAvF2JTO3JUovjqObwVSUQI1EzV5zIJ
6dDYPDf+QX9ziQpX7m72OUkbXA==
-----END PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFNTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQBsGLDk/VbxmsttzZ
iFxQcQICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEEDhQH+vLCJMnNZPU
CsFQ8RoEggTQUczalYA8v7kTy+x0I8porxMIkn+QIxZ2ErEdgyqLPWbB4Vsnd7PJ
2Ph0JoVqjpwfRDL6sXzElv0UD4Y7EWIbGOjYCrPSj3xMlhXFjglHSIQ+ZydAWtfW
I4umu5NjgyyX0sYKGMR1M1oloUIjoqoFVPLwD3axzAmggmRp5rZix7IG9A1QnrGk
PIqlC22H6yRGa0m0axSxTRChENigg7MMOJw7FzACdRvHPYeibkS5Qo1OeWUhksSP
Xxx03aMLbIkwGmRr8LFTbOb7DCMzom2UwioG1dlv1F+oP828cQDKT/OWT5KZ4474
pReemq0EBrOR9q1/8NXh3SMIpEyfpFUDRCIx1ZMaufEEysn47QWSWlWt5NOQmaZo
pNPxNbccedwgIKy3NGVqscQI47zeBq7IdMde1WOg6WCz6Fug6LB5F2FH4KvK4ukj
vXekdvWO3+cf26OOYN/H7pdenVLGpERkLLzafVynYKuiYwUgcoOhzRtwYzQJM+3C
gdN8jEh9b+aZnmxhGa2Ij67UMYVKVxna5VVNOxvB3yUfc+1uc3Etx9g/vpZ66mDI
Fqa9X+A99l0w/CmS66O2m7GLNb4MQbN/WPbDf44xbBJ8a9xsHXb92yBKam6cS1eC
MYiolJEDZ/FwYVW6106fwkmuzIndqlNQ3azAXGZuOrhvU00299n5e/P6J2CjqHr7
WArY97Eu0V0M1TqfXSIO98HRI1shJo2dIgYfqv9AChihU0hcI4ORyk5om/X9Y9m8
NOrZ1knf+XbYDcEPPUcbVioJeLs31ZJYCFhBflI0qwjrXYpId/KV2Cbf6cnDxqud
CSPNEwYXLCc4ELGm1Al0Id/j7Jc0yZXq8rVYMpfuzH3B7kzbGqfLRLckOmYTfNwr
kZOa8WHgfsRhTd+XUIEqfu+Oia0J3EE84VSShSv9JQ1qSIoZH2MeRq9rhVG5/yZ9
41LrsEx6BBBtUtNn3RdeLdtZIusOSNmO8GuqyijRG1dreJ7/dQDf+vOw8fanjZA2
TSUxMxuh6qDtUVbzBozVDv8z30QtFut1aNCfITJusfPoWjpo3QclihP3oyrrTcYn
gg8u5clFEzUnGqSmjwd7J+WrEgG0Bw8g85oXfQQ6l4QoZKZIrd0t7/VKTjd1nZUZ
+BADcEah/aKoz0stBLWXSkJ85p1osuoRX7Hb3snj1Fx7+lrmgsEJzlx+V4GbJ1Vo
NrLCa0MrqUMihsNQW0FOOhUymDZVl+ih71io4hSKVWy/vnQhpLQ09BBNnZbP2n79
J3DBiXSLNtLiODwzTMJmDsaeRGGLPImTjBrTLDpfnOlpPPkLBFQAtFtObXuZROhG
+i5bT+SdAtGg0j2k6wBJh7hEpQ57GN4w+JchIQPVkSzveyvtwYKx1D8nc2scqM8N
bgCCl2GLQXCTKOwE5ik7kjtCEqmNImHScCArSvCIFZGhY9kPhzbXE9+Z5vLvwfCi
tfsaw0RhRAYPNPS4I64/3AgiYEtUhIYxJKNVuTKJov4m/jch1UWWyd+yjtUp+AK0
whPq6czdKUH3RuTRB1UnNw697B02vUJVKDxoGhpYd1CjU8IH+2wvpLsG94pGibge
o/0YUiCWzJCBFeKQkS/skUPAeXu+i43J1BdkElUcAQWS1y02yMQ6sII=
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIERDCCAyygAwIBAgIUe1YFElSDuAZGdUVl0z8CN/bk2ZQwDQYJKoZIhvcNAQEL
BQAwejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkcnNhMjA0
OC1lbmMtcGtleS1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIyMTE5MDc0NFoX
DTM0MDMwOTE5MDc0NFowcjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTAL
BgNVBAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTElMCMG
A1UEAwwccnNhMjA0OC1lbmMtcGtleS1jZXJ0aWZpY2F0ZTCCASIwDQYJKoZIhvcN
AQEBBQADggEPADCCAQoCggEBAMYXQlhkXaSFG/qfmMNItR5z6sgcT72Q7gKXs8ou
aPV6LplTin1JahWVVKo3g1GpY4VGtn942+7wCazHBTX9/NYSuIS7Uy3B0fHSPXEU
TViM8BrOe17l6OjiK/qrmBRNk/wCkaoZkTrZLK5/FIpYjaflOqQ3tqojNpsnqaBi
OEJWkqdnqfTgoWCpNPbZTTec/DkRZ7llBWOKqaBpc0dQo6Y4LrVNQdwRJW+TOahQ
OgBo6VJMsGHFDnNm/1jldjlPjnFW2xvyCMvIW4Dvz0ZhWoIqmUChoUw6viB5S9c8
c92sbnENqwEnkfhf3bONILr8Dxj+LT2JeuNDsFk3GheDOI8CAwEAAaOByTCBxjCB
gwYDVR0RBHwweoIMKi50ZXN0LmxvY2FshwR/ChQyhwR/ChQ8hwR/ChRGhwR/ChRQ
hwR/ChRahxAAAAAAAAAAAAAAAAAAAAABhwR/f38KhwR/f38UhwR/f38ehwR/f38o
hwR/f38yhwR/f388hwR/f39GhwR/f39QhwR/f39ahwR/f39kMB0GA1UdDgQWBBRF
R7eAFOcFPGt3xFsP34G7Qw/JDDAfBgNVHSMEGDAWgBTPAtpihvTjzrMpQkvsf9HZ
eQ1P+zANBgkqhkiG9w0BAQsFAAOCAQEAaT0Nfv6mQFKnSrhmlwD/FC+FYsiw/t+x
NgT0rRfx792561p5vae4Lj6bNutKPTt+5VtJ8i9dlyAKWBA/2hk5EEkMVJVgOOY2
qahLjmK3bOhC940YDKwF/f97onqEZwqSB3xiceQHyaYOyiDojH69LsyG4VPXeUA7
rt6xUFfCATJAeDGFpdSpWRRsL4mz1bPoxyFoeUdhd7ie4dl16eXzHqFQS5vMStdj
UYwkUyBFIjgFpjps98lTyUAOss2vxFrc+Hnyh+sUtu59PwYOcWtestBtfr6hWsao
Pj0FhV8SOa0230t/lp/NzC/fQHUgSjHhs6cvgOdJFAzl02BwbO+vQA==
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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in leaf-cert.key -out leaf-cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIID0jCCArqgAwIBAgIUIK4EaHXcwU9a2fNA2inl81e81NkwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIyMTE5MTMyNFoXDTM1
MTAzMDE5MTMyNFowdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UE
AwwhcnNhMjA0OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAw5dcZa2iWVFNQMwj8BMNeLGjdNFVzkcE6OXf
aWk4McUJEg4djw1VkoetZNihrUA0VNh1jACsjoqUUaSLu8aEquSSogOyt4rfZrmm
vj3kI+aNnRn+2UQ2L6ntkWD4Gy3ipMN80Rm4Ym5GBKggdjhhPkC5W+BbPJ1nyJk4
VBsA1d2oYUOJ1ESW5VvPMUaVIe/8ANVsqzLPno5BzuRtohzPpFlaHgFM9fIOJmLQ
/A3WUVd6DhGozsOAqeb5D9rvDBNkhf5BaoG6bbZHE/V2Inf8JAsrNlqdPd5NAWTx
QcF1AoO8J2U/aAed5uFF7apKOJtfm5m9lpvqrB/dN1BnuGtr3wIDAQABo1YwVDAd
BgNVHQ4EFgQU8+iow6bZQfLd+MBRMWxjxur3BYowHwYDVR0jBBgwFoAU8+iow6bZ
QfLd+MBRMWxjxur3BYowEgYDVR0TAQH/BAgwBgEB/wIBCjANBgkqhkiG9w0BAQsF
AAOCAQEAjG2EJV14zF+rgZCV6ptoVnxjT6TMOseumdMhf/IH/s5OxeHhadVzH4+R
ySRXDkjXCNzEms12BWS0TAkJVixP9kVadkpLUKSM9vM+wGprAp9YA8kQNn0b3VaS
GeYk+GdxBDIWIZl6wTtJyCL1RKbdIbGuqJNHLp8aDOZtyz6QDTghqprR55fBmQXB
wQP4uIJKmbUJ5samErcgLlPZCied09QafILd0TCyonnx/pzF6BhKMxN3QU9+5pIu
DnLyiy2zHcBw0vjZIRQFosq1CAsO3UA7QttA/LP0euzGyNCCf5RypFFpVtWz1ApA
LMIxD+LcJWg1soH6KZnb555G7r8HQA==
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDbvCvW1hEc5LIr
NO4eGjc54dtCd2KjR9humRh3mYMt5MKc7Lme5oiSA+9Ll+5wesrChJfn2Y3EV6DH
pHYZCkteY85zr7HvbpEhMwwMPf1ux0nPoLX+l5nyPWmx5zVH4EYHHcnnbaDIsGY+
7rWOlRRxCMPwM7dDRfo14vYF1bTlGmqYRHTRQYd7XRp+OcgMQ0mMfjhCseYNwis3
pWrJTXOitc9ttV4pDFO4gAnRvTz3kjttO6/xOEnCH+JJXwn3Djmm+NGs+ToQcfQ9
q2QEb4R1i0mRREYgnhJQo0J4cbbkOTyjFXWt33j2gWRgfCi/rqmSF86407IUhR+c
KTHEP9IZAgMBAAECggEAEEr0LgDZBTBvH/igjO2nf9lsXZHmmwuXuiByvP0bMFfD
ymKPI0yES3dfn4l+oStDRe5Fws44NQFeWlwQ2aup8C/5JE2ei8D1OxkpR1Szw8QJ
IaXdklwa8Rt2oj26wj9lHqzDX/wz+mcbnbnFD1mkfPq7NJNRGMh87gPyXapj64/8
b2UAYGOWQVe1OGGcOTOe2HamzE7suJhhX2/C+WbRVIvmHntE/LYvFjLFW2brEsWW
SFR4P8jSgH/FqvltJDSGXtMyxW/fqgvSU3dN6wKplm6fK9Ejpe6yrxm7Q0SCk26c
r+TdKnUDLslmFCnIBOT30MUk8FVaWXzEQJPXxYjrNQKBgQD8O7N7VxlsGMY13CuP
O+xYIi4BCfURNyaaVKoZ9JSSb6Gz/88FfIKsj6yYbS03vSppeFEVdgDHjgJOnsOF
iL5xylHlPVoVIegVwDCdsYmF6UxWtqXOzXJGPvZfxN3QSzIqY1NDGUuJUVQy4lCJ
QtHLj/0/W4K2JKLfozeS8dTN8wKBgQDfBDprphhKKMzEXcH934075wFOZ0/ifKzY
3p+F6idFmgQ3CQ7CiyOkCqsHMvbfIn7cMRonn1o9/u9dzstuIm0FKiXlgFRvy9k9
iIfRx//p5vvdLcarS+zKETX/AIEqO7Koq4FSkpvWEt7nwxBgij6YkR1jd14juv7+
/NOjBu/GwwKBgQCXK3jAXU1BOf/hW9lHeRSHJcUInl0gOjuKl19zOuCoevshFBmR
IjR3E4zPte85zsfSemeuQxDoiLXdRM3OBEyPikBW8dRBghbEonEAsdzIQzCbb9Ak
B4GLlES41RZTth2Uxf6q4kghPnsGHBlEiqWOKPgQttm6LG7pNbsi+Ikf/QKBgHZx
Em3VmpTYMlgfy0V/ksy7lPAE4mJRAKN1KJfAkfbfI1nvT412xAj9OXPrVz2OXn2m
/lMIyK6gVJQnndbyJaBWkCXhD/cH9mo9tpRtebbJ2/3Nr/ONsUZn+Ztiszaw2ePG
ojoubuE0yHmiGXKMOU5h04/d93tWdtMJh4TPzExtAoGBAI2f7t92E0DS8Hb4Ngm6
w/hkFwEhXwVeHi/+wf2J+f8QpK3Y+4XVNChQ9ypgGwk0f3X3xlE2COBW29MABdJe
O8n3RlUlIax5GEmdMW1NGcdw1K8xaV5Ezmym9QQyV6xJ3OGmMrcSKQRPA2Zghvgo
AepEUlahMY6IuVPJXCBsVWu4
-----END PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIIFNTBfBgkqhkiG9w0BBQ0wUjAxBgkqhkiG9w0BBQwwJAQQpxTTVdrGEP+70Jq1
RDBYBAICCAAwDAYIKoZIhvcNAgkFADAdBglghkgBZQMEASoEELTSB5tqOqMP7eO2
6p8Y3sUEggTQHlc2f6670TkiCAYtJ36jDOUprPAHnODWKyGRkrpx139NoLlt2I5h
AVVc7EWgevRVDEFlLnLsALbRZEL9AW+42YwCnJBIRAvvo0VweHz6VCBZRJ1GjvSK
7d6Ys5fZLdyAC9nZEwQJTbo4Xyoo3OUnVwNro2TwfqufcCJTRwAiqvkFxguYkks7
lP3MexuxcIHrVVFw/w6uLtXgHyKg3ZZOsvz4EMgyz0WHZpUdePxxfa7YRpQwJaWJ
WbJzr9dpdPybeDuf9+NxaXWJ7q/+6+x/FT+BJ4tbKXeVHdhlXshtNCIqsY8ZnrGZ
Xk6ahXsrDzVDtqxveLpiShL2gL0t3xJMNNKQSUUobA8S0pNa4039FBeKs88MB9y0
5gKMYR3Zldlka2NBbdVjSAtTlKUpZNLl6P3JoK77YdkLKliI0SQemTdAOT3cpWzb
+fouBtAjJ5/+nPMlMDvGqm18ryOmvzq92v6XLXGRoxiVKooHSgi0aQWatKoA2ec9
UR8Y6yxc4eYN48KVaZHCT6HMtzEgMIyb0e9+qQiX6eK0EU7RUKt60xq28Sgkd/Zu
BnqQxm9W6Bk2j+OkO5VG8XEN50mai0Ib+tfLiV1wAzVyOTOnDdWo3UzDcmAPFnGW
PHhlDh6pjhIDlIL4x+RYkHXV2XQcaPq7N0K8+XI8ktMsovkZzuJ5TkGcJDmywLpi
0NpLF2UdGqWLewYYSGORcoO90HUDVedUuVw+hRomZxBazkiZ/WSp1hnj5WQr7QNq
ox3fxl9t8ahE/2SIuHDW2PZb+RPjch+Z7ejTByP1kviAYoc5s5jhZDQZSD0kED58
VypgvMl1Sy+wcWR7ukjckcl5qrA68b6AIq7p5g+uHxyR5apl4yr24dWYSGMhhyUo
+AHzXG/LGBxdf9bb9KWT6MemGx7ytggRQPQAIkdpV2s/6PTEF3jX6rkGl/ZIjeRL
y2wwDuODi3clcklgRzIgCFOrlPyS3aTOkPvSO7luUSaXz0OyfbMto8rhhNCTzZSJ
ruYAVs86t+8XK2J9dY3D9tvXoCPcy7huiXTknEL8e6G3lIUNIuWjzC5Qdz+lXhFD
hcFD7nkJGXUzqm9rl60NmZE5GI52i5oOW6eZTNLJWhjSwMUrFEniSgyoUXbzlbR2
iWBhFjWzAfZvmafuu8KCAWO71R9Ja369+TWXS+MQRd8Bm/ofLmChCGrJN77IOPlG
G5+kxgYPOV4APDgnOYYXm3BCKid9MMV+AEAitYS64CsMRmBcGngmufjvx0GCX1jl
AvwtmqSkmd6m793tP2NWbJFahzy/pbCM1vLkqpcs0nkzJNb51P7/TLbGrXsJuFP7
95WsVXRSvdklq+BnzMViYZ3ahLxtCUhs/qMX9Y56cRi7plIWcpv2eaHxalN1x4hC
H2rWf0FHzuapeEFa97mmrHi+hAwQ5n4yJYHArdrmnkmCN770zPlf5/g5xQYvcdzj
8pKkcDwsc/rTQWYqxIag3dcBw2RTtj5X/SY9mdWGY+IecAeTQDzKl9cCAzCSFRW2
+sccxUCSksgwKah0rQ3eaQSWzH7eWoL2cIYvMcx/msb3Ib/CDv69M6Z0bbkFH/cG
+VgWwbt1yBaYQGnGATwUz4o1sBPJ0MVGRk7sfEFEpgQ1+eTDSf1W01c=
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIESDCCAzCgAwIBAgIUF/mTUB7DTvia4mostOYyTRLvDvMwDQYJKoZIhvcNAQEL
BQAwfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UEAwwmcnNhMjA0
OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwHhcNMjUxMjIxMTkxNDE5
WhcNMzQwMzA5MTkxNDE5WjB0MQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTEN
MAsGA1UEBwwEQ2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMScw
JQYDVQQDDB5yc2EyMDQ4LWNoYWluLWxlYWYtY2VydGlmaWNhdGUwggEiMA0GCSqG
SIb3DQEBAQUAA4IBDwAwggEKAoIBAQDbvCvW1hEc5LIrNO4eGjc54dtCd2KjR9hu
mRh3mYMt5MKc7Lme5oiSA+9Ll+5wesrChJfn2Y3EV6DHpHYZCkteY85zr7HvbpEh
MwwMPf1ux0nPoLX+l5nyPWmx5zVH4EYHHcnnbaDIsGY+7rWOlRRxCMPwM7dDRfo1
4vYF1bTlGmqYRHTRQYd7XRp+OcgMQ0mMfjhCseYNwis3pWrJTXOitc9ttV4pDFO4
gAnRvTz3kjttO6/xOEnCH+JJXwn3Djmm+NGs+ToQcfQ9q2QEb4R1i0mRREYgnhJQ
o0J4cbbkOTyjFXWt33j2gWRgfCi/rqmSF86407IUhR+cKTHEP9IZAgMBAAGjgckw
gcYwgYMGA1UdEQR8MHqCDCoudGVzdC5sb2NhbIcEfwoUMocEfwoUPIcEfwoURocE
fwoUUIcEfwoUWocQAAAAAAAAAAAAAAAAAAAAAYcEf39/CocEf39/FIcEf39/HocE
f39/KIcEf39/MocEf39/PIcEf39/RocEf39/UIcEf39/WocEf39/ZDAdBgNVHQ4E
FgQUEPIf+S0MCRaKp3UwSU9/jqc2gqwwHwYDVR0jBBgwFoAUziID+zYERYjVp0BP
QpUWrZ9RKDcwDQYJKoZIhvcNAQELBQADggEBAAtQ021X+JtDBuA2ADs06uOnPKtt
P1AmEoTIzZkUdykpF1B4qERsxKHhJLuCxpzByIc1PxBffx/aN6HdUCLZVcEz6HW+
pDe43/+herW5GUF69Wwaf8lH2VzTrgSz6fpDVRn0uEZb9UIqDL0GfzyzMhNX6Br6
20aNN41/yw5ZZ8K2bs+tBUfpUsQbxUd2cbnjudshr2Hd29gvEvo4QJ39El+H4U13
LTdOs63JW/ep5FSOsFtQeorT9o2tAcx3DZJ4uyUdSb79w8D3SjC4WgQLGmfdnh5c
znyt/mKkpK+PXVP9JNd6eo/p/mOzVTiqgNv3RWZ8c5KBrNXIbiwBSOhtbGs=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIID1zCCAr+gAwIBAgIUSSIXFSTRZ57D7vGM5WqWHqVBjsYwDQYJKoZIhvcNAQEL
BQAwdzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkx
DjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEqMCgGA1UEAwwhcnNhMjA0
OC1jaGFpbi1jYS5yb290LWNlcnRpZmljYXRlMB4XDTI1MTIyMTE5MTM1MloXDTM0
MDMwOTE5MTM1MlowfDELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEvMC0GA1UE
AwwmcnNhMjA0OC1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUwggEiMA0G
CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDsTbJhRlpFcVedDLyEJmgY09gBbCiC
6tpH4bLIG6a+Dzcqe5hdF1McnS/NyTNPOGhNPdRKZft/7xFXjAuKclBtrA5s3k54
gxHDL7wZFh9tFzfEXwwfuFudJMg8oKRvvGcPrOXcC7kKeLZdH/JPd3jMV9KdaAsX
fzC4bDjJfLxOvIdXUD/edSz+WawrBtVOjJNsNh8/yIfDSKfeLsuwHYsTI8hjmll6
cey907ROPi83LTe0fKzLCfgjicA8DRooIZMyBEHr0EFWxUa/fL0otpGwjldvMt7B
wNqjCnI0WNKD+u6DdOJcyjZFFUtxOtotQnGvLRdMzuynAPXHzj0z3qYXAgMBAAGj
VjBUMBIGA1UdEwEB/wQIMAYBAf8CAQowHQYDVR0OBBYEFM4iA/s2BEWI1adAT0KV
Fq2fUSg3MB8GA1UdIwQYMBaAFPPoqMOm2UHy3fjAUTFsY8bq9wWKMA0GCSqGSIb3
DQEBCwUAA4IBAQBPx01cPxLlj8qFGpe97VK9u37148fkHLZlfoKcIjCZalCJ7hZQ
dY4kUz4QZ+9A03K6npODaBbIzmHNsbi9dDMwhTqjIJZqtPmr5ps08l+iTa9Ai5y0
DP9KXDyrjFPY3/o6nSrgZFbRLY19epNhjsBsmx15CzemMULSa61OBhkO/6bKF3hW
bdNC/khB3dhTYcwkRn620uH+sEKDUh76Hly8G6sxkt+IuPWz1kC2UmmFTaEmxeEp
xNjTO7zrYL/RJ1AQ9kdvm/2X3VG3xqVLZiVB3V/8sh2NPxU79w1uU04ww3PZIEqd
h9egCiFkYR9h/t5aCnsoFF3UT1LsQLl0In8j
-----END CERTIFICATE-----
)";
    return testCertificateInfo;
}

static TlsTestCertificateInfo ecDsa()
{
/*
 EC OpenSSL Commands
 ========================================================
 Root CA key: openssl ecparam -name prime256v1 -genkey -noout -out ca.key
 CA Certificate: openssl req -x509 -new -key ca.key -sha512 -days 3600 -out ca.crt -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-ca.root-certificate" -extensions v3_ca -addext "basicConstraints=critical,CA:TRUE,pathlen:10"
 Certificate key: openssl ecparam -name prime256v1 -genkey -noout -out cert.key
 Certificate sign request: openssl req -new -key cert.key -out cert.csr -subj "/C=CC/ST=City/L=City/O=Indie/OU=Indie/CN=ecdsa-certificate"
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha512
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICNjCCAdygAwIBAgIUbvcRTk2jaWV+/6/c3SBSJpicF/4wCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTE3MzhaFw0zNTEwMzAxOTE3Mzha
MG8xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxIjAgBgNVBAMMGWVjZHNhLWNh
LnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATIsaij
KRXmHry3zm0Uij9yc2XoGoZYMVS/03Ceh/6KWY8D4LPDhnPlhAmiKlPhZECbVHZ5
iDaBqAzr38Uke407o1YwVDAdBgNVHQ4EFgQUxFScSdt6X0Vx22AfQCsywOrpGCgw
HwYDVR0jBBgwFoAUxFScSdt6X0Vx22AfQCsywOrpGCgwEgYDVR0TAQH/BAgwBgEB
/wIBCjAKBggqhkjOPQQDBANIADBFAiAo+ZeQpwIK5+nDZGzTjwnhXfKlucs25C8r
VhIWnQ186AIhAKHj6R93BurVYDnnEl84X/4ksB9u5wNasaYRxMQfVhtI
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIC7INyxCwK9+fiJ0wzqSR/Ixvag1v/JCHBJof0W3ff8+oAoGCCqGSM49
AwEHoUQDQgAEp2LztBhpbEgZgY1L0gC/9T89GNw5o/wwKfC0Msbcq5imu1iW9afv
9rPUjL1GW3EK/sW7zTs+g21qeqUD5i0aFw==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICoTCCAkigAwIBAgIUExVrzvV6kHt6Gr4QKLdY/S2D2UAwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTE4MDVaFw0zNDAzMDkxOTE4MDVa
MGcxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxGjAYBgNVBAMMEWVjZHNhLWNl
cnRpZmljYXRlMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEp2LztBhpbEgZgY1L
0gC/9T89GNw5o/wwKfC0Msbcq5imu1iW9afv9rPUjL1GW3EK/sW7zTs+g21qeqUD
5i0aF6OByTCBxjCBgwYDVR0RBHwweoIMKi50ZXN0LmxvY2FshwR/ChQyhwR/ChQ8
hwR/ChRGhwR/ChRQhwR/ChRahxAAAAAAAAAAAAAAAAAAAAABhwR/f38KhwR/f38U
hwR/f38ehwR/f38ohwR/f38yhwR/f388hwR/f39GhwR/f39QhwR/f39ahwR/f39k
MB0GA1UdDgQWBBS7+SfAreP31HPHTntW1HzA8f9foDAfBgNVHSMEGDAWgBTEVJxJ
23pfRXHbYB9AKzLA6ukYKDAKBggqhkjOPQQDBANHADBEAiBeyNYIFI5oHXjV0jKI
8IWprbEgO+TS6eJQyehnDBBojgIgJAVK5jpHtNgmdyhlzi8ncCAMcJgYRocePSCL
3CMjcX8=
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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in cert.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out cert.crt -days 3000 -sha512
 Encrypt Private Key: openssl pkcs8 -topk8 -in cert.key -out cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICNjCCAdygAwIBAgIUWVMiSeK+qC0Xau+qr5skAFzaZyUwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTIwMzFaFw0zNTEwMzAxOTIwMzFa
MG8xCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxIjAgBgNVBAMMGWVjZHNhLWNh
LnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAS46Kr5
drhaJA76feoCaET+lxQECNS97YGY9/9RY5fcgBQBMAaCw3IbVSwvRHzHXlgBefZg
d15/7groblIeW8Kpo1YwVDAdBgNVHQ4EFgQUEypTnPBGdnEBzonWaK0Tq5MFk84w
HwYDVR0jBBgwFoAUEypTnPBGdnEBzonWaK0Tq5MFk84wEgYDVR0TAQH/BAgwBgEB
/wIBCjAKBggqhkjOPQQDBANIADBFAiEA5Rt3fjpypC6RvhbL7VZdt0IunxEG1C8P
9GHRQezLCXQCIEngkHtEBwD8U2ejNfaBzxuwPZYec4hY9j3l9UEVu4GS
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIE0v1hmmCNTUXA5T/yWPMYe3NtKMq1qNY6EWMJ1p+G2zoAoGCCqGSM49
AwEHoUQDQgAEhyWfDe+MNdXTdJav30qh82JKipXo7EGL0WWMqoe+xrWfroTzY5a6
yGAUFT1tET+pWNssOVPPLbVh5GBTFQAohw==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIH0MF8GCSqGSIb3DQEFDTBSMDEGCSqGSIb3DQEFDDAkBBBPmub0FjPvhK48XchG
8XuIAgIIADAMBggqhkiG9w0CCQUAMB0GCWCGSAFlAwQBKgQQU9vDzvxUmyyJ1Tvg
JhZcDASBkHnsRFCivjkxm8VTkI+ljJo6kgPSkaE7+d4F21ZqVmSJsiOtHe5QoyIz
sb42loQaYk5eBd3sIKjN6ysz/kyKiftdzCGuyjeDkDmDZN3p2kpjIrDWb77nnuxz
iH8jXrhnP6g74bf/SMitsL4tQm8qvNtUPQXor4jXVTnPNZp67Q9DRFIRRKUCXlWy
LTJIASptfA==
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICojCCAkigAwIBAgIUaX9U4lF2z1GaqOphxAOrg5X7chUwCgYIKoZIzj0EAwQw
bzELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEiMCAGA1UEAwwZZWNkc2EtY2Eu
cm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTIwNThaFw0zNDAzMDkxOTIwNTha
MGcxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxGjAYBgNVBAMMEWVjZHNhLWNl
cnRpZmljYXRlMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEhyWfDe+MNdXTdJav
30qh82JKipXo7EGL0WWMqoe+xrWfroTzY5a6yGAUFT1tET+pWNssOVPPLbVh5GBT
FQAoh6OByTCBxjCBgwYDVR0RBHwweoIMKi50ZXN0LmxvY2FshwR/ChQyhwR/ChQ8
hwR/ChRGhwR/ChRQhwR/ChRahxAAAAAAAAAAAAAAAAAAAAABhwR/f38KhwR/f38U
hwR/f38ehwR/f38ohwR/f38yhwR/f388hwR/f39GhwR/f39QhwR/f39ahwR/f39k
MB0GA1UdDgQWBBTeLGfCLmojR/AXKkz8oP5au0QM0DAfBgNVHSMEGDAWgBQTKlOc
8EZ2cQHOidZorROrkwWTzjAKBggqhkjOPQQDBANIADBFAiA8glZaxBHLSKS73rm6
fjcFzTjvgSHk6D3I9qWguTwc9wIhAJKK+5C0M14+Oq6CZPsnsnUFMH8rXYTZKc51
b0VlDt1q
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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICQjCCAeigAwIBAgIUfHkCC/cSyL8oNBbY/F36hZvO8W4wCgYIKoZIzj0EAwQw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTIzNDVaFw0zNTEwMzAx
OTIzNDVaMHUxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxKDAmBgNVBAMMH2Vj
ZHNhLWNoYWluLWNhLnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAARalx4LOCs8akQuE16aqD6sYsUXq5s9YEAlWXqOid00i7UCkxdAbrI+
LDmaCAU7dXtj5Wlr5sgVmmOKpL5BNFmTo1YwVDAdBgNVHQ4EFgQUuxZR8ZfFWcAy
EeTHRHJIgvfrAcIwHwYDVR0jBBgwFoAUuxZR8ZfFWcAyEeTHRHJIgvfrAcIwEgYD
VR0TAQH/BAgwBgEB/wIBCjAKBggqhkjOPQQDBANIADBFAiEA9TUs5c5GqOCMUOO5
v3EyzyY3m9P2CvnyFhOSPWaCH7ICIFgUt1ufcmtLASxyIIUIchSA7g2rfyF7gQHE
uskOZLRQ
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIBEThiyMzsegRrYeg3VLRYzT84vNte9oO7NZ63ap6NEaoAoGCCqGSM49
AwEHoUQDQgAEqHZL47/igVNF4BjyqBPyk6inqNeKb6hP7IcB6nOgsrCWyzdZEkRn
DpVmfBQDWAbV5SE1e4EOdQrqg6V2ajEE3w==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIICtzCCAl6gAwIBAgIUfFGl3LrkZe4Quk36Ur5XDSJhPdwwCgYIKoZIzj0EAwIw
ejELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEtMCsGA1UEAwwkZWNkc2EtY2hh
aW4taW50ZXJtZWRpYXRlLWNlcnRpZmljYXRlMB4XDTI1MTIyMTE5MjQ0MVoXDTM0
MDMwOTE5MjQ0MVowcjELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNV
BAcMBENpdHkxDjAMBgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTElMCMGA1UE
AwwcZWNkc2EtY2hhaW4tbGVhZi1jZXJ0aWZpY2F0ZTBZMBMGByqGSM49AgEGCCqG
SM49AwEHA0IABKh2S+O/4oFTReAY8qgT8pOop6jXim+oT+yHAepzoLKwlss3WRJE
Zw6VZnwUA1gG1eUhNXuBDnUK6oOldmoxBN+jgckwgcYwgYMGA1UdEQR8MHqCDCou
dGVzdC5sb2NhbIcEfwoUMocEfwoUPIcEfwoURocEfwoUUIcEfwoUWocQAAAAAAAA
AAAAAAAAAAAAAYcEf39/CocEf39/FIcEf39/HocEf39/KIcEf39/MocEf39/PIcE
f39/RocEf39/UIcEf39/WocEf39/ZDAdBgNVHQ4EFgQUwgDvN835Cwt8zIdxAwFK
0x9JT0YwHwYDVR0jBBgwFoAUNtpChL8za9AiFRlbP1n9AqOW98MwCgYIKoZIzj0E
AwIDRwAwRAIgfbO1vgUBgx8PmKO0KKur9hode7uN4wca2OcTW5jOLbkCIE9mz++B
uJG/2l6DyiyF7JB4F9nzevxq+3LilhnejVtD
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICRzCCAe2gAwIBAgIUfYUSfxxfI8TarfW3NJlI/GcUgYQwCgYIKoZIzj0EAwIw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTI0MTJaFw0zNDAzMDkx
OTI0MTJaMHoxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxLTArBgNVBAMMJGVj
ZHNhLWNoYWluLWludGVybWVkaWF0ZS1jZXJ0aWZpY2F0ZTBZMBMGByqGSM49AgEG
CCqGSM49AwEHA0IABFaoBR3LXBiN+EX/i/2UPTaE2GU9/FH5SE9sywVKTu8kEW2t
Fu1Kgj48lcDYmoH4WWLrOgKDAckFGKN8ZnvVijSjVjBUMBIGA1UdEwEB/wQIMAYB
Af8CAQowHQYDVR0OBBYEFDbaQoS/M2vQIhUZWz9Z/QKjlvfDMB8GA1UdIwQYMBaA
FLsWUfGXxVnAMhHkx0RySIL36wHCMAoGCCqGSM49BAMCA0gAMEUCIDbADlk32bzb
dxHf1smdycH1ex7TDJJwERKgYObxxfhfAiEAsZx3DOyXRxov9xhwV06cb/AbrMV9
hE6RjaEXgl22B+I=
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
 Sign Certificate: openssl x509 -req -extfile <(printf "subjectAltName=DNS:*.test.local,IP:127.10.20.50,IP:127.10.20.60,IP:127.10.20.70,IP:127.10.20.80,IP:127.10.20.90,IP:::1,IP:127.127.127.10,IP:127.127.127.20,IP:127.127.127.30,IP:127.127.127.40,IP:127.127.127.50,IP:127.127.127.60,IP:127.127.127.70,IP:127.127.127.80,IP:127.127.127.90,IP:127.127.127.100") -in leaf-cert.csr -CA intermediate-cert.crt -CAkey intermediate-cert.key -CAcreateserial -out leaf-cert.crt -days 3000 -sha256
 Encrypt Private Key: openssl pkcs8 -topk8 -in leaf-cert.key -out leaf-cert-encrypted.key
*/
    TlsTestCertificateInfo testCertificateInfo;
    testCertificateInfo.caCertificate = R"(-----BEGIN CERTIFICATE-----
MIICQjCCAeigAwIBAgIUGW36vw7awx397rvOlVgwCB/qk3gwCgYIKoZIzj0EAwQw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTI2NTRaFw0zNTEwMzAx
OTI2NTRaMHUxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARD
aXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxKDAmBgNVBAMMH2Vj
ZHNhLWNoYWluLWNhLnJvb3QtY2VydGlmaWNhdGUwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAAQIdVqIUtlBGfIOm5qZYv45ryxB4bGDx880hPt519Fmc1EUqXQem3BW
aFRyc9cKoiAQtVTkA0Nas/s9u8xQo8YFo1YwVDAdBgNVHQ4EFgQUt2O3ahRwecKI
/66iqRn7RgLfToIwHwYDVR0jBBgwFoAUt2O3ahRwecKI/66iqRn7RgLfToIwEgYD
VR0TAQH/BAgwBgEB/wIBCjAKBggqhkjOPQQDBANIADBFAiAOask3JG3MKZmuMu8Z
Xii8y+znYACvNrt0WwSQV9EK4AIhAIWcboSp/e9z5NgOpVV3thVX0gGaOydTjJna
gFsoC8Qa
-----END CERTIFICATE-----
)";
    testCertificateInfo.privateKey = R"(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIJTToYjlKsxntE1p8lgwodIzWe74c+AkkvuLjzqIvWvPoAoGCCqGSM49
AwEHoUQDQgAESjUU0N0iX0v38JzZQzVAS4+jOvExxGHiWG5m54ghSTn4k3mwNLYC
5jz0OwtKeU2MPCTX8eu1Z/bbrWGkJAAnbA==
-----END EC PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKey = R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
MIH0MF8GCSqGSIb3DQEFDTBSMDEGCSqGSIb3DQEFDDAkBBDiPAUjcln69W+Yx1V0
oX48AgIIADAMBggqhkiG9w0CCQUAMB0GCWCGSAFlAwQBKgQQi9ey902KslDxuURS
SXUxpASBkNshIIBphAPg5Mb/nrk+kbkMKpt+8RCnpJgZXSc+VULNeR1ak99km6eI
BkBP3RxBMzp0a7H6jrS9F5iFJj+4YxkY1fP5DXKzNv+VTj9ajXqMonUq/jjmPJL/
S9yvh2aNkT9aV80Kvcv4Vi9DVWtAk/wpVJFV+SkiprtdwAoQuM0BNBi08nRhySv4
AbUaUoGUvA==
-----END ENCRYPTED PRIVATE KEY-----
)";
    testCertificateInfo.encryptedPrivateKeyPassword = "password";
    testCertificateInfo.certificateChain = R"(-----BEGIN CERTIFICATE-----
MIIC0TCCAnegAwIBAgIUcjfxmpgZrng0Mw20lpmPM9NpdcYwCgYIKoZIzj0EAwIw
gYYxCzAJBgNVBAYTAkNDMQ0wCwYDVQQIDARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4w
DAYDVQQKDAVJbmRpZTEOMAwGA1UECwwFSW5kaWUxOTA3BgNVBAMMMGVjZHNhLWNo
YWluLWludGVybWVkaWF0ZS1jZXJ0aWZpY2F0ZS1wcml2YXRlLWtleTAeFw0yNTEy
MjExOTI3NTBaFw0zNDAzMDkxOTI3NTBaMH4xCzAJBgNVBAYTAkNDMQ0wCwYDVQQI
DARDaXR5MQ0wCwYDVQQHDARDaXR5MQ4wDAYDVQQKDAVJbmRpZTEOMAwGA1UECwwF
SW5kaWUxMTAvBgNVBAMMKGVjZHNhLWNoYWluLWxlYWYtY2VydGlmaWNhdGUtcHJp
dmF0ZS1rZXkwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARKNRTQ3SJfS/fwnNlD
NUBLj6M68THEYeJYbmbniCFJOfiTebA0tgLmPPQ7C0p5TYw8JNfx67Vn9tutYaQk
ACdso4HJMIHGMIGDBgNVHREEfDB6ggwqLnRlc3QubG9jYWyHBH8KFDKHBH8KFDyH
BH8KFEaHBH8KFFCHBH8KFFqHEAAAAAAAAAAAAAAAAAAAAAGHBH9/fwqHBH9/fxSH
BH9/fx6HBH9/fyiHBH9/fzKHBH9/fzyHBH9/f0aHBH9/f1CHBH9/f1qHBH9/f2Qw
HQYDVR0OBBYEFN7LXA1kMapkAf6FXvXizTjSdw7BMB8GA1UdIwQYMBaAFA/Cox+X
XMT8DIydRMPUCPjegi4fMAoGCCqGSM49BAMCA0gAMEUCIQCoQL0gS7fpZhQEuCh4
GfRHbQyLD4rOygRSuLBal0P0RAIgXyX4YiXC64guAy+zfEsPKLgV5Kkgz0WP7FoS
pQQelSI=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIICVDCCAfqgAwIBAgIUU3k8TtUACLoV/Jb1zCdSUXOQfMUwCgYIKoZIzj0EAwIw
dTELMAkGA1UEBhMCQ0MxDTALBgNVBAgMBENpdHkxDTALBgNVBAcMBENpdHkxDjAM
BgNVBAoMBUluZGllMQ4wDAYDVQQLDAVJbmRpZTEoMCYGA1UEAwwfZWNkc2EtY2hh
aW4tY2Eucm9vdC1jZXJ0aWZpY2F0ZTAeFw0yNTEyMjExOTI3MjFaFw0zNDAzMDkx
OTI3MjFaMIGGMQswCQYDVQQGEwJDQzENMAsGA1UECAwEQ2l0eTENMAsGA1UEBwwE
Q2l0eTEOMAwGA1UECgwFSW5kaWUxDjAMBgNVBAsMBUluZGllMTkwNwYDVQQDDDBl
Y2RzYS1jaGFpbi1pbnRlcm1lZGlhdGUtY2VydGlmaWNhdGUtcHJpdmF0ZS1rZXkw
WTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATqFkTTCDwYWGeEcCVuhMKXFrOQGPaG
OMBfBiFA12koZneeUAiI7++q9ViQg2AkxSguMyvoTkNa2AZyX0coUq/Qo1YwVDAS
BgNVHRMBAf8ECDAGAQH/AgEKMB0GA1UdDgQWBBQPwqMfl1zE/AyMnUTD1Aj43oIu
HzAfBgNVHSMEGDAWgBS3Y7dqFHB5woj/rqKpGftGAt9OgjAKBggqhkjOPQQDAgNI
ADBFAiEA4/mNrecQubU9bAa2ZGWLQkIB1lU9XRY3C/RksnP/uu8CIFAC2HhcFmTC
KogbBoQKsQsEj3t+Il02V/fUqq7Za1fz
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
