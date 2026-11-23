package torproc

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
	"sync"
	"time"

	"ThroneCore/internal/invoker"

	"github.com/sagernet/sing/service/filemanager"
)

const (
	tempInstancePrefix = "tmp-"
	keptInstances      = 8
	cachedFilePrefix   = "cached-"
)

var instanceNamePattern = regexp.MustCompile(`^[a-z0-9][a-z0-9_.-]{0,63}$`)

// tor locks its data directory, so each persistent directory gets one holder at a time.
var instances = struct {
	sync.Mutex
	held map[string]bool
}{held: make(map[string]bool)}

// DataDirectories hands tor its data directories. ctx is the box's: its file manager makes a root core
// leave them owned by the invoking user, who runs tor.
type DataDirectories struct {
	ctx context.Context
}

func NewDataDirectories(ctx context.Context) *DataDirectories {
	return &DataDirectories{ctx: ctx}
}

// Acquire returns key's persistent directory under tor/instances or, while another tor holds that one,
// a temporary directory seeded with its caches that release deletes.
func (d *DataDirectories) Acquire(key string) (string, func(), error) {
	root := absolute(filepath.Join(Folder, InstancesFolder))
	name := instanceName(key)
	persistent := filepath.Join(root, name)
	if claimInstance(name) {
		if err := invoker.Do(func() error { return d.create(root, persistent) }); err != nil {
			releaseInstance(name)
			return "", nil, err
		}
		var once sync.Once
		return persistent, func() { once.Do(func() { releaseInstance(name) }) }, nil
	}
	temporary := filepath.Join(root, tempInstancePrefix+randomSuffix())
	err := invoker.Do(func() error {
		if err := d.create(root, temporary); err != nil {
			return err
		}
		d.seed(persistent, temporary)
		return nil
	})
	if err != nil {
		removeInstance(temporary)
		return "", nil, err
	}
	var once sync.Once
	return temporary, func() { once.Do(func() { removeInstance(temporary) }) }, nil
}

func claimInstance(name string) bool {
	instances.Lock()
	defer instances.Unlock()
	if instances.held[name] {
		return false
	}
	instances.held[name] = true
	return true
}

func releaseInstance(name string) {
	instances.Lock()
	defer instances.Unlock()
	delete(instances.held, name)
}

func (d *DataDirectories) create(root, dir string) error {
	if err := filemanager.MkdirAll(d.ctx, root, 0o755); err != nil {
		return err
	}
	if err := filemanager.MkdirAll(d.ctx, dir, 0o700); err != nil {
		return err
	}
	// The startup pruning keeps the most recently used directories.
	now := time.Now()
	_ = os.Chtimes(dir, now, now)
	return nil
}

// A second tor starts from the first one's directory caches instead of downloading them again.
func (d *DataDirectories) seed(from, to string) {
	entries, err := os.ReadDir(from)
	if err != nil {
		return
	}
	for _, entry := range entries {
		if entry.Type().IsRegular() && strings.HasPrefix(entry.Name(), cachedFilePrefix) {
			_ = d.copyFile(filepath.Join(from, entry.Name()), filepath.Join(to, entry.Name()))
		}
	}
}

func (d *DataDirectories) copyFile(from, to string) error {
	source, err := os.Open(from)
	if err != nil {
		return err
	}
	defer source.Close()
	target, err := filemanager.OpenFile(d.ctx, to, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o600)
	if err != nil {
		return err
	}
	_, err = io.Copy(target, source)
	if closeErr := target.Close(); err == nil {
		err = closeErr
	}
	if err != nil {
		os.Remove(to)
	}
	return err
}

func removeInstance(dir string) {
	_ = invoker.Do(func() error { return os.RemoveAll(dir) })
}

// Safe names are used as they are, the rest by hash; case folds, as Windows and macOS file names do.
func instanceName(key string) string {
	name := strings.ToLower(key)
	if instanceNamePattern.MatchString(name) && !strings.HasPrefix(name, tempInstancePrefix) &&
		!strings.HasSuffix(name, ".") && filepath.IsLocal(name) {
		return name
	}
	sum := sha256.Sum256([]byte(key))
	return "k-" + hex.EncodeToString(sum[:12])
}

func randomSuffix() string {
	var suffix [8]byte
	_, _ = rand.Read(suffix[:])
	return hex.EncodeToString(suffix[:])
}

// CleanupInstances runs at core start, before any tor of this core holds a directory: it drops the temporary
// directories and the legacy data directory and keeps the keptInstances most recently used ones. It holds the
// directories' lock from before it returns until the background work ends, so no Acquire can race it.
func CleanupInstances() {
	instances.Lock()
	go func() {
		defer instances.Unlock()
		// As the invoking user: the folder is theirs to write, so it may hold links that point anywhere.
		_ = invoker.Do(cleanupInstances)
	}()
}

func cleanupInstances() error {
	_ = os.RemoveAll(filepath.FromSlash(DataDirectory))
	root := filepath.Join(Folder, InstancesFolder)
	if info, err := os.Lstat(root); err != nil || !info.IsDir() {
		return nil
	}
	entries, err := os.ReadDir(root)
	if err != nil {
		return err
	}
	type instance struct {
		name string
		used time.Time
	}
	var used []instance
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		if strings.HasPrefix(strings.ToLower(entry.Name()), tempInstancePrefix) {
			_ = os.RemoveAll(filepath.Join(root, entry.Name()))
			continue
		}
		if info, err := entry.Info(); err == nil {
			used = append(used, instance{name: entry.Name(), used: info.ModTime()})
		}
	}
	slices.SortFunc(used, func(a, b instance) int { return b.used.Compare(a.used) })
	for _, stale := range used[min(len(used), keptInstances):] {
		_ = os.RemoveAll(filepath.Join(root, stale.name))
	}
	return nil
}
