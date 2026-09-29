package warp

import (
	"context"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/x509"
	"encoding/base64"
	"encoding/hex"
	"encoding/pem"
	"net/http"
	"time"

	E "github.com/sagernet/sing/common/exceptions"
)

const (
	TunnelWireGuard = "wireguard"
	TunnelMASQUE    = "masque"
)

const (
	defaultWireGuardEndpoint = "engage.cloudflareclient.com:2408"
	// The API hands out 162.159.198.1, which is a plain CDN edge on TCP, so HTTP/2 could never reach MASQUE there.
	masqueEndpoint = "162.159.198.2:443"
)

type Identity struct {
	DeviceID      string
	Token         string
	License       string
	IPv4          string
	IPv6          string
	PrivateKey    string
	PeerPublicKey string
	Endpoint      string
	Reserved      []byte
}

func Register(ctx context.Context, tunnelType string, proxy string, apiHosts []string) (*Identity, error) {
	if tunnelType != TunnelWireGuard && tunnelType != TunnelMASQUE {
		return nil, E.New("unknown tunnel type: ", tunnelType)
	}
	apiClient, err := newClient(proxy)
	if err != nil {
		return nil, err
	}
	defer apiClient.Close()

	wgKey, err := GeneratePrivateKey()
	if err != nil {
		return nil, err
	}
	serial := make([]byte, 8)
	_, err = rand.Read(serial)
	if err != nil {
		return nil, err
	}
	apiHost, registered, err := apiClient.register(ctx, normalizeHosts(apiHosts), registerRequest{
		Key:          wgKey.PublicKey().String(),
		TOS:          time.Now().Format("2006-01-02T15:04:05.000-07:00"),
		Model:        "PC",
		SerialNumber: hex.EncodeToString(serial),
		KeyType:      "curve25519",
		TunnelType:   TunnelWireGuard,
		Locale:       "en_US",
	})
	if err != nil {
		return nil, E.Cause(err, "register device")
	}
	if registered.ID == "" || registered.Token == "" {
		return nil, E.New("register device: missing id or token in response")
	}
	identity := &Identity{
		DeviceID: registered.ID,
		Token:    registered.Token,
		License:  registered.Account.License,
	}
	if tunnelType == TunnelMASQUE {
		err = enrollMASQUE(ctx, apiClient, apiHost, registered, identity)
	} else {
		err = fillWireGuard(registered, wgKey, identity)
	}
	if err != nil {
		return nil, err
	}
	return identity, nil
}

func fillWireGuard(registered *device, wgKey Key, identity *Identity) error {
	err := fillCommon(registered, identity)
	if err != nil {
		return err
	}
	reserved, err := base64.StdEncoding.DecodeString(registered.Config.ClientID)
	if err != nil {
		return E.Cause(err, "decode client_id")
	}
	identity.PrivateKey = wgKey.String()
	identity.PeerPublicKey = registered.Config.Peers[0].PublicKey
	identity.Endpoint = registered.Config.Peers[0].Endpoint.Host
	if identity.Endpoint == "" {
		identity.Endpoint = defaultWireGuardEndpoint
	}
	identity.Reserved = reserved
	return nil
}

func enrollMASQUE(ctx context.Context, apiClient *client, apiHost string, registered *device, identity *Identity) error {
	privateKey, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return err
	}
	publicKeyDER, err := x509.MarshalPKIXPublicKey(&privateKey.PublicKey)
	if err != nil {
		return err
	}
	privateKeyDER, err := x509.MarshalECPrivateKey(privateKey)
	if err != nil {
		return err
	}
	enrolled, err := apiClient.call(ctx, apiHost, http.MethodPatch, "/reg/"+registered.ID, registered.Token, updateKeyRequest{
		Key:        base64.StdEncoding.EncodeToString(publicKeyDER),
		KeyType:    "secp256r1",
		TunnelType: TunnelMASQUE,
	})
	if err != nil {
		return E.Cause(err, "enroll MASQUE key")
	}
	err = fillCommon(enrolled, identity)
	if err != nil {
		return err
	}
	peerPublicKey, err := parsePeerPublicKey(enrolled.Config.Peers[0].PublicKey)
	if err != nil {
		return err
	}
	identity.PrivateKey = base64.StdEncoding.EncodeToString(privateKeyDER)
	identity.PeerPublicKey = peerPublicKey
	identity.Endpoint = masqueEndpoint
	return nil
}

func fillCommon(response *device, identity *Identity) error {
	addresses := response.Config.Interface.Addresses
	if addresses.V4 == "" && addresses.V6 == "" {
		return E.New("invalid response: missing interface addresses")
	}
	if len(response.Config.Peers) == 0 || response.Config.Peers[0].PublicKey == "" {
		return E.New("invalid response: missing peer public key")
	}
	identity.IPv4 = addresses.V4
	identity.IPv6 = addresses.V6
	return nil
}

func parsePeerPublicKey(content string) (string, error) {
	block, _ := pem.Decode([]byte(content))
	if block == nil {
		return "", E.New("invalid response: peer public key is not PEM")
	}
	publicKey, err := x509.ParsePKIXPublicKey(block.Bytes)
	if err != nil {
		return "", E.Cause(err, "parse peer public key")
	}
	if _, isECDSA := publicKey.(*ecdsa.PublicKey); !isECDSA {
		return "", E.New("invalid response: peer public key is not ECDSA")
	}
	return base64.StdEncoding.EncodeToString(block.Bytes), nil
}
