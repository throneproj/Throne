package rpc

import (
	"archive/tar"
	"compress/gzip"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"io"
	"os"
	"path"
	"path/filepath"
	"slices"
	"strings"

	"ThroneCore/gen"
	"ThroneCore/internal/invoker"
	"ThroneCore/internal/torproc"

	E "github.com/sagernet/sing/common/exceptions"
)

const (
	torArchiveDir     = "tor"
	torArchiveDataDir = "data"
	// tor with its libraries, plus about 26 MB of GeoIP databases.
	torMaxExtractedSize = 320 << 20
)

// Our own entries beside tor, which an archive entry must never replace.
var torReservedNames = []string{path.Base(torproc.DataDirectory), torproc.InstancesFolder, torproc.GeoIPFile, torproc.GeoIPv6File}

// Every Tor outbound of the running box switches to fresh circuits for new streams.
func (s *server) TorNewIdentity(ctx context.Context, in *gen.EmptyReq) (*gen.ErrorResp, error) {
	box := currentBox()
	if box == nil {
		return &gen.ErrorResp{Error: To("Tor is not running")}, nil
	}
	// Tor outbounds that only rules reach start on their first connection, so one that is not running yet is no failure.
	renewed := false
	var errs []error
	for _, outbound := range box.Outbound().Outbounds() {
		identity, isTor := outbound.(interface{ NewIdentity() error })
		if !isTor {
			continue
		}
		if err := identity.NewIdentity(); err != nil {
			errs = append(errs, E.Cause(err, outbound.Tag()))
		} else {
			renewed = true
		}
	}
	if renewed {
		return &gen.ErrorResp{}, nil
	}
	if len(errs) == 0 {
		return &gen.ErrorResp{Error: To("Tor is not running")}, nil
	}
	return &gen.ErrorResp{Error: To(errors.Join(errs...).Error())}, nil
}

func (s *server) InstallTor(ctx context.Context, in *gen.InstallTorRequest) (*gen.ErrorResp, error) {
	archivePath := in.GetArchivePath()
	if archivePath == "" {
		return &gen.ErrorResp{Error: To("missing archive path")}, nil
	}
	// The archive path comes from the client, and the binary later runs as the client's user: install with that user's rights.
	if err := invoker.Do(func() error { return installTor(archivePath, in.GetSha256()) }); err != nil {
		return &gen.ErrorResp{Error: To(err.Error())}, nil
	}
	return &gen.ErrorResp{}, nil
}

func installTor(archivePath, expectedSHA256 string) error {
	archive, err := os.Open(archivePath)
	if err != nil {
		return E.Cause(err, "open tor archive")
	}
	defer archive.Close()
	if expectedSHA256 != "" {
		if err = verifySHA256(archive, expectedSHA256); err != nil {
			return err
		}
	}
	gz, err := gzip.NewReader(archive)
	if err != nil {
		return E.Cause(err, "decompress tor archive")
	}
	defer gz.Close()
	if err = os.MkdirAll(torproc.Folder, 0o755); err != nil {
		return err
	}

	staged := make(map[string]string)
	defer func() {
		for _, tempPath := range staged {
			os.Remove(tempPath)
		}
	}()
	var total int64
	reader := tar.NewReader(gz)
	for {
		header, err := reader.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			return E.Cause(err, "read tor archive")
		}
		elements := strings.Split(strings.TrimPrefix(header.Name, "./"), "/")
		if strings.HasPrefix(header.Name, "/") || slices.Contains(elements, "..") {
			return E.New("invalid tor archive entry: ", header.Name)
		}
		if header.Typeflag != tar.TypeReg {
			continue
		}
		name, wanted := torInstallName(elements)
		if !wanted {
			continue
		}
		if !filepath.IsLocal(name) || strings.Contains(name, `\`) {
			return E.New("invalid tor archive entry: ", header.Name)
		}
		total += header.Size
		if total > torMaxExtractedSize {
			return E.New("tor archive is too large")
		}
		tempPath, err := stageTorFile(reader)
		if err != nil {
			return err
		}
		if previous, loaded := staged[name]; loaded {
			os.Remove(previous)
		}
		staged[name] = tempPath
	}
	if _, found := staged[torproc.ExecutableName()]; !found {
		return E.New("tor archive has no ", torproc.ExecutableName())
	}
	for name, tempPath := range staged {
		if err = os.Rename(tempPath, filepath.Join(torproc.Folder, name)); err != nil {
			return err
		}
		delete(staged, name)
	}
	return nil
}

// Where an archive entry lands in torproc.Folder: tor's own files, and the GeoIP databases from data/ beside them.
// Pluggable transports and docs sit deeper or elsewhere, so they are never taken.
func torInstallName(elements []string) (string, bool) {
	if len(elements) != 2 || elements[1] == "" {
		return "", false
	}
	name := elements[1]
	switch elements[0] {
	case torArchiveDataDir:
		return name, name == torproc.GeoIPFile || name == torproc.GeoIPv6File
	case torArchiveDir:
		lower := strings.ToLower(name)
		if strings.HasPrefix(lower, "tor-gencert") || strings.HasPrefix(name, ".") || slices.Contains(torReservedNames, lower) {
			return "", false
		}
		return name, true
	}
	return "", false
}

func verifySHA256(file *os.File, expected string) error {
	hash := sha256.New()
	if _, err := io.Copy(hash, file); err != nil {
		return E.Cause(err, "read tor archive")
	}
	if !strings.EqualFold(hex.EncodeToString(hash.Sum(nil)), strings.TrimSpace(expected)) {
		return E.New("tor archive checksum mismatch")
	}
	_, err := file.Seek(0, io.SeekStart)
	return err
}

func stageTorFile(content io.Reader) (string, error) {
	file, err := os.CreateTemp(torproc.Folder, ".install-*")
	if err != nil {
		return "", err
	}
	_, err = io.Copy(file, content)
	if err == nil {
		err = file.Chmod(0o755)
	}
	if closeErr := file.Close(); err == nil {
		err = closeErr
	}
	if err != nil {
		os.Remove(file.Name())
		return "", err
	}
	return file.Name(), nil
}
