package guard

import (
	"errors"
	"io"
	"io/fs"
	"log"
	"os"
	"strings"
	"syscall"
)

// CleanupStale flushes the pf anchors of guards that died without disarming (SIGKILL, crash, power loss).
func CleanupStale() {
	cleanupStale("")
}

func cleanupStale(own string) {
	if os.Geteuid() != 0 {
		return
	}
	ids, err := otherGuardIDs(own)
	if err != nil {
		return
	}
	for _, id := range ids {
		cleanupAnchor(id)
	}
}

func otherGuardIDs(own string) ([]string, error) {
	anchors, err := pfctl(nil, "-a", pfRootAnchor, "-s", "Anchors")
	if err != nil {
		return nil, err
	}
	var ids []string
	for line := range strings.Lines(anchors.stdout) {
		id, ok := strings.CutPrefix(strings.TrimSpace(line), pfAnchorPrefix)
		if ok && id != own && pfAnchorIDPattern.MatchString(id) {
			ids = append(ids, id)
		}
	}
	return ids, nil
}

// liveGuardAnchor runs after own's anchor is loaded: another guard still alive with a non-empty anchor proves no unguarded gap.
func liveGuardAnchor(own string) bool {
	ids, err := otherGuardIDs(own)
	if err != nil {
		return false
	}
	for _, id := range ids {
		if guardAlive(id) && anchorHasRules(pfAnchorPrefix+id) {
			return true
		}
	}
	return false
}

// A lock we can take belongs to a dead guard; closing the file releases it again.
func guardAlive(id string) bool {
	lock, err := os.OpenFile(pfLockPath(id), os.O_RDONLY|syscall.O_NOFOLLOW|syscall.O_NONBLOCK, 0)
	if err != nil {
		return false
	}
	defer lock.Close()
	return errors.Is(syscall.Flock(int(lock.Fd()), syscall.LOCK_EX|syscall.LOCK_NB), syscall.EWOULDBLOCK)
}

func anchorHasRules(anchor string) bool {
	rules, err := pfctl(nil, "-a", anchor, "-s", "rules")
	return err == nil && strings.TrimSpace(rules.stdout) != ""
}

func cleanupAnchor(id string) {
	anchor := pfAnchorPrefix + id
	lock, err := os.OpenFile(pfLockPath(id), os.O_RDONLY|syscall.O_NOFOLLOW|syscall.O_NONBLOCK, 0)
	if errors.Is(err, fs.ErrNotExist) {
		if err = flushAnchor(anchor); err != nil {
			log.Printf("kill switch: %v", err)
			return
		}
		log.Printf("kill switch: removed the orphaned pf anchor %s", anchor)
		return
	}
	if err != nil {
		return
	}
	defer lock.Close()
	if syscall.Flock(int(lock.Fd()), syscall.LOCK_EX|syscall.LOCK_NB) != nil {
		return
	}
	// Unlinked since we opened it: another cleaner already handled this guard.
	var stat syscall.Stat_t
	if syscall.Fstat(int(lock.Fd()), &stat) != nil || stat.Nlink == 0 {
		return
	}
	content, _ := io.ReadAll(io.LimitReader(lock, 128))
	if err = flushAnchor(anchor); err != nil {
		log.Printf("kill switch: %v", err)
		return
	}
	if fields := strings.Fields(string(content)); len(fields) == 2 {
		_ = releaseToken(fields[0], fields[1])
	}
	_ = os.Remove(lock.Name())
	log.Printf("kill switch: removed the pf anchor %s of a dead guard", anchor)
}
