#ifndef CERTIFICATES_H
#define CERTIFICATES_H

/*
 * certificates.h – Water Tank Cooling (Ethernet Edition)
 *
 * Uses SSLClient (BearSSL) trust-anchor format, same as DrainMaster.
 * Replace the clientCert / clientKey below with the certificate pair
 * provisioned for this specific device (WTC-AAA011) in AWS IoT Core.
 *
 * The Amazon Root CA 1 trust anchor bytes are common to all devices
 * on the same AWS account and do NOT need to change.
 */

#include <SSLClient.h>

// ── Device certificate (AWS IoT thing cert) ───────────────────────
const char clientCert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUC/EFqW/3hnn9TzBouCVpVxNg1scwDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI1MDMxMTIxMzE0
MVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALapCOlph9/siQQyn8C6
kUb3/8vKPUuBqCZUZCSEb534Zhi0x2rIy7xm862G9mQBizmG09IKwsu431/Wq1UV
QKE3nrTvrW3vJR5wmZwhORUHoK0RzHK8EKpDDbTAR0arVJnSrF+9BoEx+hAJBu+Q
cd7Ds3MsdWleFg8pxmsZqv/Dy4B27WAx/pqdLK73ZI1O/PyAFqHvZMA3KWL9uHfv
lKNaJ8Hdi/9ccmYcLll/Hedlx9jKaOTgI5JGhcCvK6F6adNlTJqpkM4Tx0BtOISf
aVSGqa8aFScPBNTH0BzWqbSKxNTPZCgh5Kyp8MUiZeQEU7XXchlQ1kMoa8CyQ0Fc
HVcCAwEAAaNgMF4wHwYDVR0jBBgwFoAUy5EwFoFzTpwUAsV+0wIy+jio9ZMwHQYD
VR0OBBYEFFq6tzOMOOtYZ44Rcu0zaiDMn7GJMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQC37lqd2TwnuNlHBehJH1xUUS0U
18dYrNOtAdPJcs5M5+B+n/AkCWlpJGnABzepmAqNhr7KVQ7neWYEEVoHpTjBdnES
ZxD8sXyL9Ie/wL8Lx9jqIZM24EQJWfvnJoL3mL9eEeTMDJ7WDshZfpkqVP4BFkP5
KD6Pjl54tFzRgI6fbzyb+Jxm2PFdq9lyCslGho7FO7WSGxqb4lQi71ckkzYuzWhK
tW+/7hEeRjuCHQly/5bE+UL6Rv+xCeb4VLYKSMeKkLwbYMHNENU1wmO+zVfYP/9m
Xq3hB4w0yFljJogzno+r021jjlFAMT48beI9QffE+e6PpAGIehSA4NJHTN1z
-----END CERTIFICATE-----
)EOF";

// ── Device private key ────────────────────────────────────────────
const char clientKey[] PROGMEM = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAtqkI6WmH3+yJBDKfwLqRRvf/y8o9S4GoJlRkJIRvnfhmGLTH
asjLvGbzrYb2ZAGLOYbT0grCy7jfX9arVRVAoTeetO+tbe8lHnCZnCE5FQegrRHM
crwQqkMNtMBHRqtUmdKsX70GgTH6EAkG75Bx3sOzcyx1aV4WDynGaxmq/8PLgHbt
YDH+mp0srvdkjU78/IAWoe9kwDcpYv24d++Uo1onwd2L/1xyZhwuWX8d52XH2Mpo
5OAjkkaFwK8roXpp02VMmqmQzhPHQG04hJ9pVIaprxoVJw8E1MfQHNaptIrE1M9k
KCHkrKnwxSJl5ARTtddyGVDWQyhrwLJDQVwdVwIDAQABAoIBAHrieWZeYtTY0s0K
KcOFQFtYWLSHWHlFvxQaTkzq9BR4mmcgp9BFShtzv5gMZhKdn0aSWErEhox70Xsu
dpGE/Lf5LUJYxHpjGrvB0PXiu/5T5VrJ0JuXvjZtafkiKlF2zjG2M9Us3AVq0+qZ
yBq/OHw/eKiRTmQWsgx9dEl1OT9bGtdD/bh6e155mdVaSATpsB/KYhDMftz4Kh3/
DYNoM8dqAbNFxbftTrgGsrgC/T38WoKm1AI5z1QHNrtczsN8bVhU6yuWtU2dGQWD
s7hNgpHYvbgEIhTNgvf2vT4RviMtoAQ0EDqXZqwEsZFAczNSDc/BmQrio9YXHznE
bEhA41ECgYEA8qL2/MSccNqKQcSq66HkiltNTjRdmoUdToyTdHEEnHzyBSSzg6Lv
z7kDQD9dNfT+TLq81jL9um/FOuefCxt6kIpVZJRL8z+94qsBZZiJ+0IvWoNBubob
AvOnzD0yfaxEhXHtAE3nKfKMXgi0AU73d4EpjgWYWZxXaaliJM5cpHUCgYEAwLhw
aHhrh0+0Y6JnhZIGDPzOq+OS1kmxLCXre2qgGJAkDjzsA/wdcf/92FvxW6kwZkFb
9E3pmfp2LLiwkO6Kc7qM9yc09Fu/qCjise5Fd12PV7a4oYONvIUzimC1sbkT6V9m
bESOftUhUJpBVVYRWQHye2L0kP4gR6JCiCqfERsCgYAtGAR3LcM1ZihT2M07RbdH
z3gqlKjg0uSDeLTe6zJEMyR3uD50tI+FN4lXI2+bW5D3ia0W0hs9zxAExo9UbSL2
Qf9k1frXln0f51A3JYZfYAmU9Nf+QIxMnCQPXUBJAv8pHedCKzhPH3je8RcjNx3e
4+5pKrkJznigdo568K9fEQKBgFWXFEUxhf/4RBMj43oM2icWd+sbDPGilM8YoDaV
qjh+e6TfJaq3Y5RnrqNSYiTlRRuE14PuvlqmQ6mk9LXJWy/+n/B8NyZ3QO08C0Ie
ojdbE/hOrDz/Igmh1rwUK12c5tz0g5Z99BMcMMmNWIq/yMCQ/tIRprBmTIvD4mx7
EV4VAoGBAK86mTS3m2wcbM3qJChAKQbzaPHcV3K/5sQj4z93T2T8DxB4t7S9S1EO
9W+ikfL2/7XxOncxupMlHjdqg/L8fcWUNBMSwu5TJ9G1VLuM0pVFBirbqbpe7rsd
5x+ZtAkt7jbkshteXxWYGR/P5aoi1dhDW+ZW5FUQM3Ztp5bZ2N24
-----END RSA PRIVATE KEY-----
)EOF";

// ── Amazon Root CA 1 – Trust Anchor (BearSSL DER format) ─────────
// Common for all AWS IoT endpoints; does NOT change per device.

const unsigned char TAs_0_DN[] PROGMEM = {
  0x30, 0x39, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x55, 0x53, 0x31, 0x0f, 0x30, 0x0d, 0x06, 0x03, 0x55, 0x04, 0x0a,
  0x13, 0x06, 0x41, 0x6d, 0x61, 0x7a, 0x6f, 0x6e, 0x31, 0x19, 0x30, 0x17,
  0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x10, 0x41, 0x6d, 0x61, 0x7a, 0x6f,
  0x6e, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41, 0x20, 0x31,
};

const unsigned char TAs_0_RSA_N[] PROGMEM = {
  0xb2, 0x78, 0x80, 0x71, 0xca, 0x78, 0xd5, 0xe3, 0x71, 0xaf, 0x47, 0x80,
  0x50, 0x74, 0x7d, 0x6e, 0xd8, 0xd7, 0x88, 0x76, 0xf4, 0x99, 0x68, 0xf7,
  0x58, 0x21, 0x60, 0xf9, 0x74, 0x84, 0x01, 0x2f, 0xac, 0x02, 0x2d, 0x86,
  0xd3, 0xa0, 0x43, 0x7a, 0x4e, 0xb2, 0xa4, 0xd0, 0x36, 0xba, 0x01, 0xbe,
  0x8d, 0xdb, 0x48, 0xc8, 0x07, 0x17, 0x36, 0x4c, 0xf4, 0xee, 0x88, 0x23,
  0xc7, 0x3e, 0xeb, 0x37, 0xf5, 0xb5, 0x19, 0xf8, 0x49, 0x68, 0xb0, 0xde,
  0xd7, 0xb9, 0x76, 0x38, 0x1d, 0x61, 0x9e, 0xa4, 0xfe, 0x82, 0x36, 0xa5,
  0xe5, 0x4a, 0x56, 0xe4, 0x45, 0xe1, 0xf9, 0xfd, 0xb4, 0x16, 0xfa, 0x74,
  0xda, 0x9c, 0x9b, 0x35, 0x39, 0x2f, 0xfa, 0xb0, 0x20, 0x50, 0x06, 0x6c,
  0x7a, 0xd0, 0x80, 0xb2, 0xa6, 0xf9, 0xaf, 0xec, 0x47, 0x19, 0x8f, 0x50,
  0x38, 0x07, 0xdc, 0xa2, 0x87, 0x39, 0x58, 0xf8, 0xba, 0xd5, 0xa9, 0xf9,
  0x48, 0x67, 0x30, 0x96, 0xee, 0x94, 0x78, 0x5e, 0x6f, 0x89, 0xa3, 0x51,
  0xc0, 0x30, 0x86, 0x66, 0xa1, 0x45, 0x66, 0xba, 0x54, 0xeb, 0xa3, 0xc3,
  0x91, 0xf9, 0x48, 0xdc, 0xff, 0xd1, 0xe8, 0x30, 0x2d, 0x7d, 0x2d, 0x74,
  0x70, 0x35, 0xd7, 0x88, 0x24, 0xf7, 0x9e, 0xc4, 0x59, 0x6e, 0xbb, 0x73,
  0x87, 0x17, 0xf2, 0x32, 0x46, 0x28, 0xb8, 0x43, 0xfa, 0xb7, 0x1d, 0xaa,
  0xca, 0xb4, 0xf2, 0x9f, 0x24, 0x0e, 0x2d, 0x4b, 0xf7, 0x71, 0x5c, 0x5e,
  0x69, 0xff, 0xea, 0x95, 0x02, 0xcb, 0x38, 0x8a, 0xae, 0x50, 0x38, 0x6f,
  0xdb, 0xfb, 0x2d, 0x62, 0x1b, 0xc5, 0xc7, 0x1e, 0x54, 0xe1, 0x77, 0xe0,
  0x67, 0xc8, 0x0f, 0x9c, 0x87, 0x23, 0xd6, 0x3f, 0x40, 0x20, 0x7f, 0x20,
  0x80, 0xc4, 0x80, 0x4c, 0x3e, 0x3b, 0x24, 0x26, 0x8e, 0x04, 0xae, 0x6c,
  0x9a, 0xc8, 0xaa, 0x0d,
};

const unsigned char TAs_0_RSA_E[] PROGMEM = {
  0x01, 0x00, 0x01,
};

const br_x509_trust_anchor TAs[] PROGMEM = {
  {
    { (unsigned char*)TAs_0_DN, sizeof(TAs_0_DN) },
    BR_X509_TA_CA,
    {
      BR_KEYTYPE_RSA,
      { .rsa = {
          (unsigned char*)TAs_0_RSA_N, sizeof(TAs_0_RSA_N),
          (unsigned char*)TAs_0_RSA_E, sizeof(TAs_0_RSA_E),
      } }
    }
  },
};

const size_t TAs_NUM = 1;

// ── Mutual TLS parameters (mTLS) ─────────────────────────────────
SSLClientParameters mTLS = SSLClientParameters::fromPEM(
  clientCert, sizeof(clientCert),
  clientKey,  sizeof(clientKey)
);

#endif // CERTIFICATES_H
