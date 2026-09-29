package rpc

import (
	"archive/zip"
	"context"
	"io"
	"os"
	"path/filepath"
	"strings"

	"ThroneCore/gen"

	E "github.com/sagernet/sing/common/exceptions"
)

func (s *server) InstallDashboard(ctx context.Context, in *gen.InstallDashboardRequest) (*gen.ErrorResp, error) {
	archivePath := in.GetArchivePath()
	targetDir := in.GetTargetDir()
	if archivePath == "" || targetDir == "" {
		return &gen.ErrorResp{Error: To("missing archive path or target dir")}, nil
	}

	reader, err := zip.OpenReader(archivePath)
	if err != nil {
		return &gen.ErrorResp{Error: To(E.Cause(err, "open dashboard archive").Error())}, nil
	}
	defer reader.Close()

	trimDir := zipIsInSingleDirectory(reader.File)
	tempDir := targetDir + ".tmp"
	if err = os.RemoveAll(tempDir); err != nil {
		return &gen.ErrorResp{Error: To(err.Error())}, nil
	}
	if err = os.MkdirAll(tempDir, 0o755); err != nil {
		return &gen.ErrorResp{Error: To(err.Error())}, nil
	}

	failed := func(err error) (*gen.ErrorResp, error) {
		os.RemoveAll(tempDir)
		return &gen.ErrorResp{Error: To(err.Error())}, nil
	}
	var extracted int
	for _, file := range reader.File {
		if file.FileInfo().IsDir() {
			continue
		}
		elements := strings.Split(file.Name, "/")
		if trimDir {
			elements = elements[1:]
		}
		if len(elements) == 0 {
			continue
		}
		relativePath := filepath.Join(elements...)
		if !filepath.IsLocal(relativePath) {
			return failed(E.New("invalid dashboard archive entry: ", file.Name))
		}
		savePath := filepath.Join(tempDir, relativePath)
		if err = os.MkdirAll(filepath.Dir(savePath), 0o755); err != nil {
			return failed(err)
		}
		if err = extractZipEntry(file, savePath); err != nil {
			return failed(err)
		}
		extracted++
	}
	if extracted == 0 {
		return failed(E.New("dashboard archive is empty"))
	}
	if err = adoptExtracted(tempDir); err != nil {
		return failed(E.Cause(err, "hand the dashboard files to the invoking user"))
	}

	if err = os.RemoveAll(targetDir); err != nil {
		return failed(err)
	}
	if err = os.Rename(tempDir, targetDir); err != nil {
		return failed(err)
	}
	return &gen.ErrorResp{}, nil
}

func extractZipEntry(file *zip.File, savePath string) error {
	reader, err := file.Open()
	if err != nil {
		return err
	}
	defer reader.Close()
	writer, err := os.Create(savePath)
	if err != nil {
		return err
	}
	defer writer.Close()
	_, err = io.Copy(writer, reader)
	return err
}

// GitHub archives wrap every file under a single "<repo>-<branch>/" top-level directory.
func zipIsInSingleDirectory(files []*zip.File) bool {
	var dirName string
	for _, file := range files {
		if file.FileInfo().IsDir() {
			continue
		}
		elements := strings.Split(file.Name, "/")
		if len(elements) < 2 {
			return false
		}
		if dirName == "" {
			dirName = elements[0]
		} else if dirName != elements[0] {
			return false
		}
	}
	return dirName != ""
}
