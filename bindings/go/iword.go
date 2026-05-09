// Package iword provides Go bindings for the iWord C library via cgo.
//
// The iWord shared memory dictionary must be loaded before calling Seek or Map:
//
//	iwordctl load words.txt
//
// or programmatically via Load().
package iword

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../bin -liword -lc

#include "iword.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// Mode flags for Map().
const (
	ModeHTML    = C.IWORD_MODE_HTML
	ModeForbid  = C.IWORD_MODE_FORBID
	ModeEnglish = C.IWORD_MODE_ENGLISH
)

// Category key constants.
const (
	KeyHidden  = C.IWORD_KEY_HIDDEN
	KeyAdult   = C.IWORD_KEY_ADULT
	KeySpam    = C.IWORD_KEY_SPAM
	KeyDefault = 9
)

// Match represents a word found in text by Map().
type Match struct {
	Position int // byte offset in source text
	Length   int // byte length of matched word
	Key      int // category key (0-14)
}

// Load reads a dictionary file into shared memory.
// Returns an error if loading fails.
func Load(filename string) error {
	cs := C.CString(filename)
	defer C.free(unsafe.Pointer(cs))
	if C.iword_load(cs) != 0 {
		return fmt.Errorf("iword: failed to load dictionary: %s", filename)
	}
	return nil
}

// Unload releases the shared memory dictionary.
func Unload() error {
	if C.iword_unload() != 0 {
		return fmt.Errorf("iword: failed to unload dictionary")
	}
	return nil
}

// Seek searches for a single word in the loaded dictionary.
// Returns the category key (0-14), or -1 if not found.
func Seek(word string) int {
	cs := C.CString(word)
	defer C.free(unsafe.Pointer(cs))
	return int(C.iword_seek(cs))
}

// Map extracts all matching words from text.
// mode is a combination of ModeHTML, ModeForbid, ModeEnglish.
func Map(text string, mode int) []Match {
	cs := C.CString(text)
	defer C.free(unsafe.Pointer(cs))

	raw := C.iword_map(cs, C.int(len(text)), C.int(mode))
	if raw == nil {
		return nil
	}
	defer C.free(unsafe.Pointer(raw))

	var matches []Match
	for i := 0; ; i++ {
		entry := *(*C.longlong)(unsafe.Pointer(
			uintptr(unsafe.Pointer(raw)) + uintptr(i)*8,
		))
		if entry == 0 {
			break
		}
		matches = append(matches, Match{
			Position: int(entry>>16) & 0xFFFFFFFF,
			Key:      int(entry>>8) & 0xFF,
			Length:   int(entry) & 0xFF,
		})
	}
	return matches
}

// SetLimit sets the maximum number of matches returned by Map.
func SetLimit(n int) {
	C.iword_set_limit(C.int(n))
}

// SetDictKey selects which dictionary to use (for multiple dictionaries).
func SetDictKey(key string) {
	cs := C.CString(key)
	defer C.free(unsafe.Pointer(cs))
	C.iword_set_strkey(cs, C.size_t(len(key)))
}

// Mask returns a bitmask of category keys present in the loaded dictionary.
func Mask() int {
	return int(C.iword_mask())
}

// FilterText replaces all matched words in text with '*' characters.
func FilterText(text string, mode int) string {
	matches := Map(text, mode)
	if len(matches) == 0 {
		return text
	}
	buf := []byte(text)
	for _, m := range matches {
		for i := 0; i < m.Length; i++ {
			buf[m.Position+i] = '*'
		}
	}
	return string(buf)
}
