package iword_test

// Integration tests — requires a pre-loaded dictionary:
//
//	bin/iwordctl load dict/spam_en.txt
//	go test ./bindings/go/
//
// The CI workflow loads the dictionary before running tests.

import (
	"os"
	"testing"

	iword "github.com/0xkaz/iword/bindings/go"
)

// dictKey is the shared memory key used by the test dictionary.
// Override with IWORD_TEST_KEY env var to avoid collisions in parallel CI.
func dictKey() string {
	if k := os.Getenv("IWORD_TEST_KEY"); k != "" {
		return k
	}
	return ""
}

func TestMain(m *testing.M) {
	if k := dictKey(); k != "" {
		iword.SetDictKey(k)
	}
	os.Exit(m.Run())
}

func TestSeekFound(t *testing.T) {
	key := iword.Seek("free")
	if key != iword.KeySpam {
		t.Errorf("Seek(\"free\") = %d, want %d (KeySpam)", key, iword.KeySpam)
	}
}

func TestSeekNotFound(t *testing.T) {
	key := iword.Seek("notaword_xyz_abc")
	if key != -1 {
		t.Errorf("Seek(\"notaword_xyz_abc\") = %d, want -1", key)
	}
}

func TestMapReturnsMatches(t *testing.T) {
	matches := iword.Map("Get your free prize now!", iword.ModeHTML|iword.ModeForbid)
	if len(matches) == 0 {
		t.Fatal("Map returned no matches for spam text")
	}
	for _, m := range matches {
		if m.Position < 0 {
			t.Errorf("match has negative Position: %+v", m)
		}
		if m.Length <= 0 {
			t.Errorf("match has non-positive Length: %+v", m)
		}
		if m.Key < 0 || m.Key > 14 {
			t.Errorf("match has out-of-range Key: %+v", m)
		}
	}
}

func TestMapEmptyText(t *testing.T) {
	matches := iword.Map("", iword.ModeHTML|iword.ModeForbid)
	if len(matches) != 0 {
		t.Errorf("Map(\"\") returned %d matches, want 0", len(matches))
	}
}

func TestFilterText(t *testing.T) {
	input := "This is free spam"
	output := iword.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if output == input {
		t.Error("FilterText did not modify spam text")
	}
	found := false
	for _, c := range output {
		if c == '*' {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("FilterText output contains no '*': %q", output)
	}
}

func TestFilterTextClean(t *testing.T) {
	input := "hello world"
	output := iword.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if output != input {
		t.Errorf("FilterText modified clean text: got %q, want %q", output, input)
	}
}
